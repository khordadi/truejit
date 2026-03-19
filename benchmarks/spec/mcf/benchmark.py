from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'mcf'
binary = directory / 'mcf.wasm'
workloads = [Workload(name=None, args=[directory / 'inp.in'], pre_opens=[directory])]
mcf = Benchmark(name, binary, workloads)


def build():
    raise NotImplementedError(f'build() for `{name}` not implemented')
