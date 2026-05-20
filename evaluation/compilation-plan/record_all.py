"""
Record jit + interp profiles for the first workload of every benchmark.
Profiles land in /users/khordadi/truejit/profiles/<benchmark>/<workload>/{jit,interp}/<timestamp>/
modelled on profiling.record_base(), but limited to one workload per benchmark.
"""
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from benchmarks import all_benchmarks
from utils import VirtualMachine

MODES = [
    ('jit',   []),
    ('interp', ['--interp=all']),
]


def profiles_root(binary, workload=None, mode=None):
    root = Path(str(binary).replace('/benchmarks/', '/profiles/')).parent
    if workload is not None:
        root = root / workload
    if mode is not None:
        root = root / mode
    root.mkdir(parents=True, exist_ok=True)
    return root


def record(vm, binary, workload, options, profile_path):
    get_dst = lambda: Path(profile_path) / datetime.now(ZoneInfo("Europe/London")).strftime("%Y-%m-%d_%H-%M-%S")
    dst = get_dst()
    while dst.exists():
        time.sleep(1)
        dst = get_dst()
    dst.mkdir(parents=True, exist_ok=True)

    # Clean up any leftover SQLite db files so each run starts fresh.
    subprocess.run(['find', '/users/khordadi/truejit/benchmarks/sqlite/workloads',
                    '-name', 'db.sqlite*', '-delete'], check=True)

    vm.run(binary, workload, options=options, env={'METRICS_DIR': str(dst)})


def main(reps=1):
    vm = VirtualMachine()  # cmake is a no-op if already built

    ok, fail = [], []
    total = len(all_benchmarks) * len(MODES) * reps

    counter = 0
    for benchmark in all_benchmarks:
        wl = benchmark.workloads[0]

        for mode, options in MODES:
            for rep in range(reps):
                counter += 1
                label = f'[{benchmark.suite}] {benchmark.name}/{wl.name}/{mode}'
                if reps > 1:
                    label += f' rep={rep + 1}'
                print(f'\n[{counter}/{total}] {label}', flush=True)
                try:
                    path = profiles_root(benchmark.binary, wl.name, mode)
                    record(vm, benchmark.binary, wl, options[:], path)
                    ok.append(label)
                    print('  OK', flush=True)
                except Exception as e:
                    fail.append((label, str(e)))
                    print(f'  FAIL: {e}', flush=True)

    print(f'\n{"=" * 60}')
    print(f'PASS: {len(ok)}   FAIL: {len(fail)}')
    if fail:
        print('\nFailures:')
        for label, reason in fail:
            print(f'  {label}: {reason}')


if __name__ == '__main__':
    reps = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    main(reps)
