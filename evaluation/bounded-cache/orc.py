#!/usr/bin/env python3

import sys
import json
import os
import subprocess
from pathlib import Path
import argparse

sys.path.append("/tmp/truejit/evaluation")
from utility import Reporter, Recorder, metric

parser = argparse.ArgumentParser(description="Orchestrator build and run script")
parser.add_argument(
    "--size", "-s", "-n",
    type=int,
    default=1,
    help="Size of the cluster"
)
parser.add_argument(
    "--reps", "-r",
    type=int,
    default=1,
    help="Number of repetitions for each configuration"
)
parser.add_argument(
    "--interval", "-i",
    type=int,
    default=None,
    help="Interval in ms between runs (default: None, no interval)"
)
parser.add_argument(
    "--benchmark",
    type=str,
    default="jetstream/gcc_loops",
    help="Benchmark to run (default: jetstream/gcc_loops)"
)
parser.add_argument(
    "--sync",
    type=str,
    default="jit",
    help="Synchronization strategy (default: jit)"
)
parser.add_argument(
    "--compiler",
    type=str,
    default="llvm",
    help="Compiler to use llvm or clift (default: llvm)"
)
parser.add_argument(
    "--network-latency",
    type=int,
    default=0,
    help="Network latency in ms (default: 0)"
)


def orc(cluster_size=1,
        release_interval=None,
        sync_strategy="jit",
        global_cache=None,
        local_cache=None,
        benchmark="jetstream/gcc_loops",
        remote_compilation=True
        ):
    cmd = []

    binary = Path("/usr/local/bin/orchestrator")
    if not binary.exists():
        raise FileNotFoundError(f"Orchestrator binary {binary} does not exist.")
    cmd.append(str(binary))

    cmd += [f"--size={cluster_size}"]
    if release_interval is not None:
        cmd.append(f"--interval={release_interval}")

    cmd += [f"--benchmark={benchmark}"]

    cmd += [f"--sync={sync_strategy}"]

    if global_cache is not None:
        cmd += [f"--global-cache={global_cache}"]

    if local_cache is not None:
        cmd += [f"--local-cache={local_cache}"]

    cmd += [f"--remote-compilation={str(remote_compilation).lower()}"]

    print(' '.join(cmd))
    subprocess.run(cmd, stdout=sys.stdout, stderr=subprocess.STDOUT, text=True, check=True)


def main():
    args = parser.parse_args()

    reporter = Reporter("/tmp/result.csv")

    configs = [
        "global",
        "local",
        "compilation_strategy"
    ]
    collectables = [
        "waiting",
        "e2e",
        "exec",
        "global-cache.misses",
        "global-cache.compilations",
        "global-cache.evictions"
    ]

    recorder = Recorder(configs, collectables)
    reporter.append(recorder.header())

    for _ in range(args.reps):
        for global_cache in [500, 1000, 1500, 2000, 2100, 2200, 2300, 2400, 2500, None][::-1]:
            for local_cache in [500, 1000, 1500, 2000, 2500, None][::-1]:
                for compilation_strategy in ["predictive.lstm-50", "jit"]:
                    orc(cluster_size=args.size,
                        release_interval=args.interval,
                        benchmark=args.benchmark,
                        sync_strategy=compilation_strategy,
                        global_cache=global_cache,
                        local_cache=local_cache)

                    report = recorder.record([global_cache, local_cache, compilation_strategy])
                    reporter.append(report)


if __name__ == '__main__':
    main()

"""
*** Artifact Evaluation Note ***
sudo cmake --build /tmp/truejit/release --target install -j64
cd /tmp/truejit/evaluation/bounded-cache
./orc.py --benchmark=ffmpeg --size=10 --reps=5 --interval=10000 2>&1 | tee report.txt
cat /tmp/result.csv 
mv /tmp/result.csv /tmp/bounded-cache.benchmark=ffmpeg.size=100.reps=5.interval=50.csv
"""
