import subprocess
from pathlib import Path

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'quicksort'
binary = directory / 'quicksort.wasm'
workloads = [Workload()]
quicksort = Benchmark(name, binary, workloads)


def install():
    if not src_dir.exists():
        src_dir.mkdir()
        subprocess.run(['wget', '-N', 'https://browserbench.org/JetStream/wasm/quicksort.c'], check=True,
                       cwd=src_dir)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O3',
             '-z', 'stack-size=32768',
             '-Wl,--initial-memory=1048576',
             '-Wl,--export=main',
             '-Wl,--export=__heap_base',
             '-Wl,--export=__data_end',
             '-Wl,--allow-undefined',
             '-Wno-implicit-function-declaration',
             '-fno-exceptions']
    sources = ['quicksort.c']
    subprocess.run([compiler, *flags, *sources, '-o', binary], cwd=src_dir, check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)


if __name__ == '__main__':
    src_dir = directory / 'src'

    install()
    build()
    uninstall()
