import json
import time
from os import cpu_count
import pulp
import pandas as pd

all_modes = ['interpret', 'jit', 'static', 'specialize', 'async']


class ObjectiveFunction:
    calculator: dict
    priorities: list[str]

    def __init__(self):
        self.profile = None
        self.penalties = {
            mode: i * 0.0001 for i, mode in enumerate(self.priorities)
        }

    def value(self, fn, mode):
        return self.calculator[mode](fn)

    def set_profile(self, profile):
        self.profile = profile

    def __call__(self, fn, mode, consider_priorities=True):
        if self.profile is None:
            raise ValueError("[solver] set the profile first!")

        val = self.value(fn, mode)
        if consider_priorities:
            val += self.penalties[mode]

        return val

    def __getitem__(self, key):
        fn, mode = key
        return self.value(fn, mode)

    def __str__(self):
        return self.__class__.__name__


class EndToEndTime(ObjectiveFunction):
    priorities = ['static', 'async', 'specialize', 'jit', 'interpret']

    def __init__(self):
        super().__init__()
        self.calculator = {
            'interpret': lambda fn: self.profile.loc[fn, 'exec.interp'],  # FIXME: this is a heuristic!
            'jit': lambda fn: self.profile.loc[fn, 'exec.jit'] + self.profile.loc[fn, 'compilation.jit'],
            'static': lambda fn: self.profile.loc[fn, 'exec.jit'],
            'specialize': lambda fn: self.profile.loc[fn, 'exec.spec'] + self.profile.loc[fn, 'compilation.spec'],
            'async': lambda fn: self.profile.loc[fn, 'exec.jit'] + .5 * self.profile.loc[fn, 'compilation.jit'],
            # fixme: always assume bgjit hides **half** of compilation time
        }


class StaticCodeSize(ObjectiveFunction):
    priorities = ['async', 'specialize', 'jit', 'static', 'interpret']

    def __init__(self):
        super().__init__()
        self.calculator = {
            'interpret': lambda fn: 0,
            'jit': lambda fn: 0,
            'static': lambda fn: self.profile.loc[fn, 'size.static'],
            'specialize': lambda fn: 0,
            'async': lambda fn: 0,
        }


class DynamicCodeSize(ObjectiveFunction):
    priorities = ['interpret', 'static', 'async', 'jit', 'specialize']

    def __init__(self):
        super().__init__()
        self.calculator = {
            'interpret': lambda fn: 0,
            'jit': lambda fn: self.profile.loc[fn, 'size.dynamic.jit'],
            'static': lambda fn: self.profile.loc[fn, 'size.static'],
            'specialize': lambda fn: self.profile.loc[fn, 'size.dynamic.spec'],
            'async': lambda fn: self.profile.loc[fn, 'size.dynamic.jit'],
        }


class CompilationTime(ObjectiveFunction):
    priorities = ['static', 'async', 'jit', 'interpret', 'specialize']

    def __init__(self):
        super().__init__()
        self.calculator = {
            'interpret': lambda fn: 0,
            'jit': lambda fn: self.profile.loc[fn, 'compilation.jit'],
            'static': lambda fn: 0,
            'specialize': lambda fn: self.profile.loc[fn, 'compilation.spec'],
            'async': lambda fn: 0,
        }


class StartupTime(ObjectiveFunction):
    priorities = ['static', 'jit', 'specialize', 'async', 'interpret']

    def __init__(self, profile):
        super().__init__()
        self.calculator = {
            mode: lambda fn: self.profile.loc[fn, 'startup'] for mode in all_modes
        }


class Constraint:
    def __init__(self, objective, lower_bound=None, upper_bound=None):
        if lower_bound is not None and upper_bound is not None and lower_bound > upper_bound:
            raise ValueError(f"lower_bound {lower_bound} > upper_bound {upper_bound}")
        if lower_bound is not None and lower_bound < 0:
            raise ValueError(f"lower_bound {lower_bound} < 0")
        if upper_bound is not None and upper_bound < 0:
            raise ValueError(f"upper_bound {upper_bound} < 0")
        self.objective = objective
        self.lower_bound = lower_bound
        self.upper_bound = upper_bound

    def __call__(self, fn, mode):
        return self.objective(fn, mode, consider_priorities=False)

    def set_profile(self, profile):
        self.objective.set_profile(profile)

    def __str__(self):
        s = ''
        s += f'{self.lower_bound} <= ' if self.lower_bound is not None else ''
        s += str(self.objective)
        s += f' <= {self.upper_bound}' if self.upper_bound is not None else ''
        return s


class Solver:
    def __init__(self, profile):
        self.plan = None
        self.status = None
        self.profile = profile

    def solve(self, goal, constraints=None, log=False, timeout=None):
        print(f'[goal] {goal}')
        goal.set_profile(self.profile)
        problem_name = str(goal)
        prob = pulp.LpProblem(problem_name, pulp.LpMinimize)

        # Only select one mode for each function
        fns = range(len(self.profile))
        modes = range(len(all_modes))

        # Decision variables (a table of size |fns| x |modes|)
        choices = pulp.LpVariable.dicts("select", (fns, modes), cat="Binary")
        for fn in fns:
            prob += pulp.lpSum([choices[fn][mode] for mode in modes]) == 1

        # Objective function
        objective = [[goal(fn, mode) for mode in all_modes] for fn in fns]
        prob += pulp.lpSum([objective[fn][mode] * choices[fn][mode] for fn in fns for mode in modes])

        # Global Constraints
        for constraint in constraints or []:
            print(f'[constraint] {constraint}')
            constraint.set_profile(self.profile)
            objective = [[constraint(fn, mode) for mode in all_modes] for fn in fns]
            if constraint.upper_bound is not None:
                prob += pulp.lpSum(
                    [objective[fn][mode] * choices[fn][mode] for fn in fns for mode in modes]) <= constraint.upper_bound
            if constraint.lower_bound is not None:
                prob += pulp.lpSum(
                    [objective[fn][mode] * choices[fn][mode] for fn in fns for mode in modes]) >= constraint.lower_bound

        # Solve the problem
        print("Solving...")
        start = time.time()
        prob.solve(pulp.PULP_CBC_CMD(
            msg=log,
            timeLimit=timeout,
            threads=cpu_count() - 2,
            # gapRel=0.01  # stop when within 1% of optimal
        ))
        end = time.time()
        self.status = pulp.LpStatus[prob.status]
        print(f'[status] {self.status}')
        print(f'[elapsed time] {end - start:.2f} seconds')

        # Output results
        decisions = [(self.profile.loc[fn, 'id'], all_modes[mode]) for fn in fns for mode in modes if
                     pulp.value(choices[fn][mode]) == 1]

        self.plan = pd.DataFrame(decisions, columns=['id', 'mode'])

    def to_json(self, path):
        asyncs = []  # to maintain the order
        for i, row in self.plan.iterrows():
            if row['mode'] == 'async':
                asyncs.append(row['id'])
        with open(path, 'w') as f:
            f.write(json.dumps({
                'async': asyncs,
                'interpret': self.plan[self.plan['mode'] == 'interpret']['id'].tolist(),
                'specialize': self.plan[self.plan['mode'] == 'specialize']['id'].tolist(),
                'static': self.plan[self.plan['mode'] == 'static']['id'].tolist(),
                'jit': self.plan[self.plan['mode'] == 'jit']['id'].tolist(),
            }))

    def stats(self):
        return self.plan['mode'].value_counts().to_dict()

    def ids(self):
        return set(self.plan['id'].tolist())

    def set_defaults(self, defaults):
        self.plan = pd.concat([self.plan, defaults])
