import ast
import subprocess
import re
from pathlib import Path
import shutil
from datetime import datetime
from zoneinfo import ZoneInfo
import time

from solver import *
from utils import metric, mv_profile, static_compile, VirtualMachine, ffmpeg, gcc_loops


def generate_static_info(binary):
    objdump = "/usr/local/bin/wasm-objdump"
    proc = subprocess.run([objdump, "-d", str(binary)],
                          capture_output=True, text=True, check=True)
    lines = proc.stdout.splitlines()
    last_line = lines[-1]
    assert "end" in last_line, f"last line of objdump output is {last_line}"
    last_address = int(last_line.split(":")[0].strip(), 16)

    # match lines like "0000001a func[3] <function_name>:"
    pattern = re.compile(r"^([0-9A-Fa-f]+)\s+func\[(\d+)\]\s+<(.*)>:")
    funcs = []
    for line in lines:
        m = pattern.search(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        fid = int(m.group(2))
        name = str(m.group(3))
        funcs.append((fid, name, addr))

    # get the binary size from static-compiler output
    bin_dir = Path(str(binary).replace("/benchmarks/", "/binaries/")).parent
    if not bin_dir.exists() or not any(bin_dir.iterdir()):
        print(f"binary directory {bin_dir} does not exist; trying to compile it.")
        static_compile(binary)
    bin_sizes = {int(f.stem): f.stat().st_size for f in bin_dir.glob("*.o")}  # {fid: size}

    parsed = []
    for i, (fid, name, addr) in enumerate(funcs):
        end_addr = funcs[i + 1][2] if i + 1 < len(funcs) else last_address
        size = end_addr - addr
        assert size > 0, f"function {name} has size 0"
        parsed.append((fid, name, addr, size, bin_sizes.get(fid)))

    df = pd.DataFrame(parsed, columns=["id", "name", "start", "size.bytecode", "size.static"])
    df = df.astype(
        {"id": "int64", "start": "int64", "size.bytecode": "int64", "size.static": "int64", "name": "string"})
    df = df[["id", "name", "size.bytecode", "size.static"]]
    # sort by id
    df = df.sort_values(by='id', ascending=True)

    # write to file
    df.to_csv(profiles_root(binary) / 'metadata.csv', index=False)


def get_static_info(binary):
    df = pd.read_csv(profiles_root(binary) / 'metadata.csv')
    df = df.reset_index(drop=True)
    return df


# FIXME: clean it up
class StartupInfo:
    def __init__(self, metrics_path):
        # start_time
        start_time = int(metric(metrics_path / 'start.txt'))

        # (id, start)
        j = ast.literal_eval(metric(metrics_path / 'startups.txt'))
        df = pd.DataFrame(j, columns=['id', 'start'])
        df['start'] = df['start'] - start_time

        # sort by start
        df = df.sort_values(by='start', ascending=True)

        # drop index and use id as index
        self.df = df.reset_index(drop=True)

    def get_startup_fns(self, compilation_info):
        df = pd.merge(self.df, compilation_info.df, on='id', how='outer')

        # calculate deficit
        df['compilation.end'] = df['start'] + df['compilation']

        # sort by start
        df = df.sort_values(by='start', ascending=True)

        # compiler_idle_time[0] = 0
        # compiler_idle_time[i] = start[i] - compilation.end[i-1]
        df['idle'] = (df['start'] - df['compilation.end'].shift(1, fill_value=0)).astype('int64').mask(df.index == 0, 0)
        df['deficit'] = df['compilation'] - df['idle']
        # deficit.cum[i] = SUM(deficit[0:i])
        df['deficit.cum'] = df['deficit'].cumsum()
        df['deficit.cum.diff'] = df['deficit.cum'].diff().fillna(df['deficit.cum']).astype('int64')
        max_cum_deficit = df['deficit.cum'].max()

        # 'critical' is true if ('deficit.cum.diff' is positive and 'deficit.cum' is less that half of deficit_cum_max)
        df['critical'] = (df['deficit.cum.diff'] > 0) & (df['deficit.cum'] < (max_cum_deficit / 2))

        df = df[['id', 'start', 'compilation', 'idle', 'deficit', 'deficit.cum', 'critical']]
        # print(df)

        # everything from i == 0 to the **first** location that 'critical' is false is 'startup = True'
        startup = []
        for i, row in df.iterrows():
            if not row['critical']:
                break
            startup.append((row['id'], row['start']))
        return startup

    def get_startup(self, fn):
        df = self.df[self.df['id'] == fn]
        if len(df) == 0:
            # raise ValueError(f"function {fn} not found in startup information")
            return 0
        return int(df['start'].iloc[0])


def profiles_root(binary, workload=None, mode=None):
    root = Path(str(binary).replace('/benchmarks/', '/profiles/')).parent

    if workload is not None:
        root = root / workload

    if mode is not None:
        root = root / mode

    if not root.exists():
        root.mkdir(parents=True, exist_ok=True)
    return root


def plans_root(binary, workload=None, mode=None):
    root = Path(str(binary).replace('/benchmarks/', '/plans/')).parent

    if workload is not None:
        root = root / workload

    if mode is not None:
        root = root / mode

    if not root.exists():
        root.mkdir(parents=True, exist_ok=True)
    return root


def plan_json_to_df(plan_json):
    plan = json.load(open(plan_json))
    rows = []
    for mode, fns in plan.items():
        for fn in fns:
            rows.append((fn, mode))
    return pd.DataFrame(rows, columns=['id', 'mode'])


def get_profile(profile_path):
    # (id, freq, exec)
    id_freq_exec = profile_path / 'functions-frequency-and-duration.txt'
    id_freq_exec = ast.literal_eval(metric(id_freq_exec))
    id_freq_exec = pd.DataFrame(id_freq_exec, columns=['id', 'freq', 'exec'])

    # start_time
    start_time = int(metric(profile_path / 'start.txt'))

    # (id, start)
    startup_info = StartupInfo(profile_path)
    id_start = startup_info.df[['id', 'start']]

    # (id, end)
    # id_end = profile_path / 'ends.txt'
    # id_end = ast.literal_eval(metric(id_end))
    # id_end = pd.DataFrame(id_end, columns=['id', 'end'])
    # id_end['end'] -= start_time
    # id_end = id_end[['id', 'end']]

    # (id, start, end, freq, exec)
    df = pd.merge(id_start, id_freq_exec, on='id', how='left')
    df.fillna(0, inplace=True)  # FIXME: it seems only the _start function is not recorder;

    # (id, compilation, size.dynamic)
    if int(metric(profile_path / 'waiting.txt')):
        j = ast.literal_eval(metric(profile_path / 'waiting-and-sizes.txt'))
        j = [(v[0], v[2], v[3]) for v in j]
    else:
        j = []
    compilation = pd.DataFrame(j, columns=['id', 'compilation', 'size.dynamic'])
    df = pd.merge(df, compilation, on='id', how='left')
    df.fillna(0, inplace=True)

    df = df.sort_values(by='start', ascending=True)
    df = df.reset_index(drop=True)

    df = df.astype({'id': 'int64', 'start': 'int64', 'exec': 'int64', 'freq': 'int64', 'compilation': 'int64',
                    'size.dynamic': 'int64'})

    return df[['id', 'start', 'exec', 'freq', 'compilation', 'size.dynamic']]


class Recorder:
    def __init__(self):
        self.vm = VirtualMachine()

    # runs a benchmark/workload/mode and saves the records in its directory
    def record(self, binary, workload, options, profile_path, env=None):
        get_dst = lambda: Path(profile_path) / datetime.now(ZoneInfo("Europe/London")).strftime("%Y-%m-%d_%H-%M-%S")
        dst = get_dst()
        while dst.exists():
            time.sleep(1)
            dst = get_dst()
        dst.mkdir(parents=True, exist_ok=True)

        if env is None:
            env = {}
        env['METRICS_DIR'] = str(dst)

        self.vm.run(binary, workload, options=options, env=env)


def record_base(benchmark, rep=1):
    recorder = Recorder()

    for wl in benchmark.workloads:
        if wl.name != 'mp4-flip-vertical':
            continue
        print(f'[workload] {wl.name}')
        for mode, options in [
            ('jit', []),
            # ('spec', ['--specialize=planned']),
            # ('interp', ['--interp=all']),
        ]:
            print(f'[mode] {mode}')
            if mode == 'spec':
                shutil.copy(profiles_root(benchmark.binary, wl.name) / 'spec_candidates.json', '/tmp/plan.json')
            profile_path = profiles_root(benchmark.binary, wl.name, mode)
            for r in range(rep):
                print(f'[rep] {r + 1}/{rep}')
                recorder.record(benchmark.binary, wl, options, profile_path)


def concat_profiles(profiles):
    result = profiles[0]
    for profile in profiles[1:]:
        result = pd.concat([result, profile])
    result = result.groupby(['id', 'name']).agg(lambda x: int(x.mean()))
    result = result.sort_values(by='start.jit', ascending=True)
    result['id'] = result.index.get_level_values('id')
    result['name'] = result.index.get_level_values('name')
    result = result.reset_index(drop=True)
    return result


def average_profiles(profiles_path):
    profiles = []
    for profile_path in profiles_path.iterdir():
        if profile_path.is_dir():
            profiles.append(get_profile(profile_path))

    profile = pd.concat(profiles)
    profile = profile.groupby('id').agg(lambda x: int(x.mean()))
    profile = profile.sort_values(by='start', ascending=True)
    profile['id'] = profile.index.get_level_values('id')
    profile = profile.reset_index(drop=True)
    return profile


def generate_profile_base(benchmark):
    for wl in benchmark.workloads:
        print(f'[workload] {wl.name}')

        jit = average_profiles(profiles_root(benchmark.binary, wl.name, 'jit'))
        jit = jit[['id', 'start', 'exec', 'freq', 'compilation', 'size.dynamic']]
        jit.rename(inplace=True, columns={
            'start': 'start.jit',
            'exec': 'exec.jit',
            'freq': 'freq.jit',
            'compilation': 'compilation.jit',
            'size.dynamic': 'size.dynamic.jit'
        })

        interp = average_profiles(profiles_root(benchmark.binary, wl.name, 'interp'))
        interp = interp[['id', 'start', 'exec', 'freq']]
        interp.rename(inplace=True, columns={
            'start': 'start.interp',
            'exec': 'exec.interp',
            'freq': 'freq.interp',
        })

        df = pd.merge(jit, interp, on='id', how='outer')

        if (spec_candidates_path := profiles_root(benchmark.binary, wl.name) / "spec_candidates.json").exists():
            spec = average_profiles(profiles_root(benchmark.binary, wl.name, "spec"))
            spec = spec[['id', 'start', 'exec', 'freq', 'compilation', 'size.dynamic']]
            spec.rename(inplace=True, columns={
                'start': 'start.spec',
                'exec': 'exec.spec',
                'freq': 'freq.spec',
                'compilation': 'compilation.spec',
                'size.dynamic': 'size.dynamic.spec'
            })
            df = pd.merge(df, spec, on='id', how='outer')
        else:
            df['exec.spec'] = 999999999999
            df['compilation.spec'] = 999999999999
            df['size.dynamic.spec'] = 999999999999

        df = df.sort_values(by=f'start.jit', ascending=True)

        static_info = get_static_info(benchmark.binary)
        df = pd.merge(df, static_info, on='id', how='left')

        df.to_csv(profiles_root(benchmark.binary, wl.name) / 'profile.csv',
                  index=False,
                  columns=[
                      'id',
                      'name',
                      'size.bytecode',
                      'size.static',

                      'start.jit',
                      'exec.jit',
                      'freq.jit',
                      'compilation.jit',
                      'size.dynamic.jit',

                      'start.interp',
                      'exec.interp',
                      'freq.interp',

                      'exec.spec',
                      'compilation.spec',
                      'size.dynamic.spec'
                  ])


def generate_profile_leave_one_out(benchmark):
    for wl in benchmark.workloads:
        print(f'[workload] {wl.name}')
        profiles = []
        for training_on_wl in benchmark.workloads:
            if wl.name == training_on_wl.name:
                continue

            profile_path = profiles_root(benchmark.binary, training_on_wl.name) / 'profile.csv'
            profiles.append(pd.read_csv(profile_path))

        concat_profile = concat_profiles(profiles)
        concat_profile.to_csv(profiles_root(benchmark.binary, wl.name) / 'leave_one_out.csv', index=False, columns=[
            'id',
            'name',
            'size.bytecode',
            'size.static',

            'start.jit',
            'exec.jit',
            'freq.jit',
            'compilation.jit',
            'size.dynamic.jit',

            'start.interp',
            'exec.interp',
            'freq.interp',

            'exec.spec',
            'compilation.spec',
            'size.dynamic.spec'
        ])


def generate_profile_union(benchmark):
    profiles = []
    for wl in benchmark.workloads:
        profile_path = profiles_root(benchmark.binary, wl.name) / 'profile.csv'
        profiles.append(pd.read_csv(profile_path))
    profile = concat_profiles(profiles)
    profile.to_csv(profiles_root(benchmark.binary) / 'union.csv', index=False, columns=[
        'id',
        'name',
        'size.bytecode',
        'size.static',

        'start.jit',
        'exec.jit',
        'freq.jit',
        'compilation.jit',
        'size.dynamic.jit',

        'start.interp',
        'exec.interp',
        'freq.interp',

        'exec.spec',
        'compilation.spec',
        'size.dynamic.spec'
    ])


def generate_profile_oracle(benchmark):
    for wl in benchmark.workloads:
        print(f'[workload] {wl.name}')
        profile_path = profiles_root(benchmark.binary, wl.name) / 'profile.csv'
        dest_path = profiles_root(benchmark.binary, wl.name) / 'oracle.csv'
        shutil.copy(profile_path, dest_path)


def generate_plans(benchmark):
    binary_size = Path(benchmark.binary).stat().st_size
    print(f'[binary_size] {binary_size}')

    static_info = get_static_info(benchmark.binary)
    all_ids = set(static_info['id'].tolist())

    # per-workload plans
    for wl in benchmark.workloads:
        # if wl.name != 'mp4-to-pngs':
        # if wl.name != 'mp4-gaussian-blur':
        #     continue
        print(f'[workload] {wl.name}')
        for plan, goal, constraints, default in [
            (
                    'min_e2e.unconstrained.default_static',
                    EndToEndTime(),
                    [],
                    'static'
            ),
            (
                    'min_e2e.unconstrained.default_jit',
                    EndToEndTime(),
                    [],
                    'jit'
            ),
            # (
            #         'min_e2e.file_size_lt_5percent',
            #         EndToEndTime(),
            #         [Constraint(StaticCodeSize(), upper_bound=binary_size * .05)],
            #         'jit'
            # )
        ]:
            print(f'[plan] {plan}')

            profiles = []
            # all pairs
            for training_on_wl in benchmark.workloads:
                profiles.append((f'trained_on_{training_on_wl.name}',
                                 profiles_root(benchmark.binary, training_on_wl.name) / 'profile.csv'))
            # oracle
            profiles.append(('oracle', profiles_root(benchmark.binary, wl.name) / 'oracle.csv'))
            # leave-one-out
            profiles.append(('leave_one_out', profiles_root(benchmark.binary, wl.name) / 'leave_one_out.csv'))
            # union
            profiles.append(('union', profiles_root(benchmark.binary) / 'union.csv'))

            for profile_name, profile_path in profiles:
                print(f'[profile] {profile_name}')
                profile = pd.read_csv(profile_path)
                solver = Solver(profile)
                solver.solve(
                    goal=goal,
                    constraints=constraints,
                    log=False,
                    timeout=10
                )
                pre_stats = solver.stats()

                # add defaults
                default_ids = all_ids - solver.ids()
                defaults = pd.DataFrame([(fn, default) for fn in default_ids], columns=['id', 'mode'])
                solver.set_defaults(defaults)

                post_stats = solver.stats()
                print(f'[plan] {pre_stats} ==[default:{default}]==> {post_stats}')

                plan_path = plans_root(benchmark.binary, wl.name, plan) / f'{profile_name}.json'
                if not plan_path.parent.exists():
                    plan_path.parent.mkdir(parents=True, exist_ok=True)
                solver.to_json(plan_path)


def run_plans(benchmark):
    recorder = Recorder()

    # per-workload plans
    for wl in benchmark.workloads:
        # if wl.name != 'mp4-to-pngs':
        #     continue
        print(f'[workload] {wl.name}')
        for plan in [
            'min_e2e.unconstrained.default_static',
            'min_e2e.unconstrained.default_jit',
            # 'min_e2e.file_size_lt_5percent'
        ]:
            print(f'[plan] {plan}')

            plans_datasets = []
            # oracle
            plans_datasets.append('oracle')
            # leave-one-out
            plans_datasets.append('leave_one_out')
            # union
            plans_datasets.append('union')

            # all pairs
            plans_datasets.extend([f'trained_on_{x.name}' for x in benchmark.workloads])

            for plan_dataset in plans_datasets:
                print(f'[plan] {plan_dataset}')
                plan_path = plans_root(benchmark.binary, wl.name, plan) / f'{plan_dataset}.json'

                recorder.record(benchmark.binary,
                                wl,
                                options=['--interp=planned', '--async=planned', '--static=planned',
                                         '--specialize=planned'],
                                profile_path=profiles_root(benchmark.binary, wl.name, plan) / plan_dataset,
                                env={'PLAN': plan_path})


def spec_candidates(benchmark):
    vm = VirtualMachine(['PROFILE_FOR_SPECIALIZATION=On'])

    for wl in benchmark.workloads:
        vm.run(benchmark.binary, wl)

        profile_path = profiles_root(benchmark.binary, wl.name)
        if not profile_path.exists():
            profile_path.mkdir(parents=True, exist_ok=True)
        shutil.copy('/tmp/plan.json', profile_path / 'spec_candidates.json')


class Profile:
    def __init__(self, profile_path):
        self.e2e = []
        self.exec = []
        self.waiting = []
        self.compilation = []

        self.dynamic_code_size = []
        self.static_code_size = []

        for path in profile_path.iterdir():
            if path.is_dir() and any(path.iterdir()):
                self.e2e.append(int(metric(path / 'e2e.txt')))
                self.exec.append(int(metric(path / 'exec.txt')))
                self.waiting.append(int(metric(path / 'waiting.txt')))
                self.compilation.append(int(metric(path / 'compilation.txt')))

                self.dynamic_code_size.append(sum(eval(metric(path / 'dynamic-sizes.txt'))))
                self.static_code_size.append(sum(eval(metric(path / 'static-sizes.txt'))))


def get_history(benchmark_binary, workload_name, mode='jit'):
    profile_path = profiles_root(benchmark_binary, workload_name, mode)
    for path in profile_path.iterdir():
        if path.is_dir() and any(path.iterdir()):
            startups = eval(metric(path / 'startups.txt'))
            return [i for (i, _) in startups]
    raise RuntimeError(f'No startups for {workload_name}')


def main():
    benchmark = ffmpeg
    # benchmark = gcc_loops
    # generate_static_info(benchmark.binary)
    # return

    # collect profiles for jit and interp
    # record_base(benchmark, 100)

    # merge the profiles
    # generate_profile_base(benchmark)
    # return

    generate_profile_leave_one_out(benchmark)
    generate_profile_union(benchmark)
    generate_profile_oracle(benchmark)
    # return

    # generate plans
    # generate_plans(benchmark)

    run_plans(benchmark)

    return


if __name__ == '__main__':
    main()
