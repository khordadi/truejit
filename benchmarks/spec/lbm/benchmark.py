from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'lbm'
binary = directory / 'lbm.wasm'
workloads = [Workload(name=None, args=['64', directory / 'reference.dat', '0', '1', directory / '100_100_130_ldc.of'],
                      pre_opens=[directory])]
lbm = Benchmark(name, binary, workloads)


def build():
    raise NotImplementedError(f'build() for `{name}` not implemented')
