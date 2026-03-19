from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'nab'
binary = directory / 'nab.wasm'
workloads = [Workload(name=None, args=['aminos', '391519156', '1000'], pre_opens=[directory])]
nab = Benchmark(name, binary, workloads)


def build():
    raise NotImplementedError(f'build() for `{name}` not implemented')
