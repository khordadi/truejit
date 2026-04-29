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
    priorities = ['jit', 'static', 'async', 'specialize', 'interpret']

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


class PlanEvaluation:
    def __init__(self, static_info, base_profile, plan, history=None):
        e2e = 0
        exec = 0
        waiting = 0
        compilation = 0
        dynamic_code_size = 0

        # merge on both ('id', 'name')
        df = pd.merge(static_info, base_profile, on=['id', 'name', 'size.bytecode', 'size.static'], how='outer')
        print(f'[plan evaluation] merged static_info | base_profile: {df.shape[0]} rows')

        df = pd.merge(df, plan, on='id', how='outer')
        print(f'[plan evaluation] merged static_info | base_profile | plan: {df.shape[0]} rows')
        df.drop('name', axis=1, inplace=True)
        df.fillna(0, inplace=True)

        # iterate the rows of df
        for fn in history:
            row = df[df['id'] == fn].iloc[0]
            mode = row['mode']
            if mode == 'interpret':
                e2e += row['exec.interp']
                exec += row['exec.interp']
                waiting += 0
                compilation += 0
                dynamic_code_size += 0
            elif mode == 'jit':
                e2e += row['exec.jit'] + row['compilation.jit']
                exec += row['exec.jit']
                waiting += row['compilation.jit']
                compilation += row['compilation.jit']
                dynamic_code_size += row['size.dynamic.jit']
            elif mode == 'static':
                e2e += row['exec.jit']  # like jit without compilation/waiting
                exec += row['exec.jit']  # like jit
                waiting += 0
                compilation += 0
                dynamic_code_size += row['size.dynamic.jit']  # also add the dynamic code
            elif mode == 'async':
                e2e += row['exec.jit'] + .5 * row[
                    'compilation.jit']  # assume async compilation happens in parallel with execution, so we only add half of the compilation time to e2e
                exec += row['exec.jit']
                waiting += .5 * row[
                    'compilation.jit']  # assume async compilation happens in parallel with execution, so we only add half of the compilation time to waiting
                compilation += row['compilation.jit']
                dynamic_code_size += row['size.dynamic.jit']
            elif mode == 'specialize':
                e2e += row['exec.spec'] + row['compilation.spec']
                exec += row['exec.spec']
                waiting += row['compilation.spec']
                compilation += row['compilation.spec']
                dynamic_code_size += row['size.dynamic.spec']
            else:
                raise ValueError(f'Unknown mode: {mode}')

        # merge static_info and plan on 'id' to get the static code size of all functions in the plan
        df = pd.merge(static_info, plan, on='id', how='inner')
        static_code_size = 0
        for i, row in df.iterrows():
            if row['mode'] == 'static':
                static_code_size += row['size.static']

        plan_modes = plan['mode'].value_counts().to_dict()
        self.jit = plan_modes.get('jit', 0)
        self.interpret = plan_modes.get('interpret', 0)
        self.async_compilation = plan_modes.get('async', 0)
        self.specialize = plan_modes.get('specialize', 0)
        self.static = plan_modes.get('static', 0)

        self.e2e = e2e
        self.exec = exec
        self.waiting = waiting
        self.compilation = compilation
        self.dynamic_code_size = dynamic_code_size
        self.static_code_size = static_code_size

    def print(self):
        print({
            'jit': self.jit,
            'interpret': self.interpret,
            'async_compilation': self.async_compilation,
            'specialize': self.specialize,
            'static': self.static,
            'e2e': self.e2e,
            'exec': self.exec,
            'waiting': self.waiting,
            'compilation': self.compilation,
            'dynamic_code_size': self.dynamic_code_size,
            'static_code_size': self.static_code_size,
        })
        # return json.dumps({
        #     'jit': self.jit,
        #     'interpret': self.interpret,
        #     'async_compilation': self.async_compilation,
        #     'specialize': self.specialize,
        #     'static': self.static,
        #     'e2e': self.e2e,
        #     'exec': self.exec,
        #     'waiting': self.waiting,
        #     'compilation': self.compilation,
        #     'dynamic_code_size': self.dynamic_code_size,
        #     'static_code_size': self.static_code_size,
        # }, indent=2)


class Planner:
    def __init__(self, goal, constraints, default):
        self.goal = goal
        self.constraints = constraints
        self.default = default

    def plan(self, static_info, base_profile):
        all_ids = static_info['id'].tolist()
        if self.goal is None:
            return pd.DataFrame({'id': all_ids, 'mode': [self.default] * len(all_ids)})
        solver = Solver(base_profile)
        solver.solve(self.goal, self.constraints, timeout=10)
        plan = solver.plan
        unplanned_ids = set(all_ids) - set(plan['id'].tolist())
        defaults = pd.DataFrame({'id': list(unplanned_ids), 'mode': [self.default] * len(unplanned_ids)})
        return pd.concat([plan, defaults])






if __name__ == '__main__':
    import os
    import sys
    from pathlib import Path
    from solver import *
    from profiling import *
    from utils import *
    import numpy as np

    benchmark = ffmpeg
    static_info = get_static_info(benchmark.binary)

    wl = ffmpeg.workloads[0]
    profile = pd.read_csv(profiles_root(benchmark.binary, wl.name) / 'profile.csv')

    planner = Planner(EndToEndTime(), [Constraint(StaticCodeSize(), upper_bound=100_000)], 'interpret')
    plan = planner.plan(static_info, profile)
    # summary of df
    print(plan['mode'].value_counts())

    history = get_history(benchmark.binary, wl.name)

    evaluation = PlanEvaluation(static_info, profile, plan, history)
    evaluation.print()