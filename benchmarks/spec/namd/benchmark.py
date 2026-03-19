from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'namd'
binary = directory / 'namd.wasm'
workloads = [Workload(name=None, args=['--input', directory / 'apoa1.input', '--iterations', '2'], pre_opens=[directory])]
namd = Benchmark(name, binary, workloads)


def build():
    raise NotImplementedError(f'build() for `{name}` not implemented')
