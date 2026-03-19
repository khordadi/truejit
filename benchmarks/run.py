import json
import subprocess
from pathlib import Path

from Benchmarking import TrueJIT
from coremark.benchmark import benchmark
from ffmpeg.benchmark import ffmpeg
from jetstream.suite import jetstream
from mibench.suite import mibench
from spec.suite import spec
from sqlite.benchmark import sqlite
from wabench.suite import wabench
from polybench.suite import suite as polybench
from npb.suite import suite as npb

# TODO:
# 1. Implement train function
# 2. Implement multi-workload benchmarks
# 3. Implement bash command generation


if __name__ == '__main__':
    truejit = TrueJIT()

    benchmarks = []
    benchmarks.extend(jetstream.benchmarks)
    benchmarks.extend([benchmark])
    benchmarks.extend([sqlite])
    benchmarks.extend([ffmpeg])
    benchmarks.extend(mibench.benchmarks)
    benchmarks.extend(spec.benchmarks)
    benchmarks.extend(npb('S').benchmarks)
    benchmarks.extend(polybench('extra-large').benchmarks)
    benchmarks.extend(wabench.benchmarks)

    # filter benchmarks
    # benchmarks = [b for b in benchmarks if b.name == 'gcc-loops']

    for i, benchmark in enumerate(benchmarks):
        print(f'{benchmark.binary}')

    exit(0)

    schemes = {
        'baseline': [],
        'async': ['--async=plan', '--threads=1'],
        'static (20%)': [f'--static=startup.20%'],
        'async + static (20%)': ['--async=plan', f'--static=startup.20%'],
        'static oracle': [f'--static=startup.100%'],
    }
    repetitions = 10
    results = {}
    for benchmark in benchmarks:
        results[benchmark.name] = {}

        for scheme, vm_options in schemes.items():
            print(f'Running {benchmark.name} with scheme {scheme}')
            results[benchmark.name][scheme] = {'compilations': [], 'waitings': [], 'execs': [], 'e2es': []}

            for i in range(repetitions):
                profile_file = '/tmp/profile.json'
                env = {'PROFILE': profile_file}
                truejit.run(benchmark.binary, benchmark.workloads[0], vm_options, env)

                profile = json.load(open(profile_file))
                results[benchmark.name][scheme]['compilations'].append(profile['compilation'])
                results[benchmark.name][scheme]['waitings'].append(profile['waiting'])
                results[benchmark.name][scheme]['execs'].append(profile['exec'])
                results[benchmark.name][scheme]['e2es'].append(profile['end-to-end'])
        print(f'results={results}')
        with open('/tmp/results.json', 'w') as f:
            json.dump(results, f, indent=4)
    print(f'results={results}')
    with open('/tmp/results.json', 'w') as f:
        json.dump(results, f, indent=4)
