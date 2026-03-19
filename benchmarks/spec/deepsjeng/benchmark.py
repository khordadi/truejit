from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'deepsjeng'
binary = directory / 'deepsjeng.wasm'
workloads = [Workload(name=None, args=[directory / 'train.txt'], pre_opens=[directory])]
deepsjeng = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang++'
    flags = [
        '-z', 'stack-size=1048576', # stack 1MB
        '-Wno-unsupported-floating-point-opt',
        '-Wno-expansion-to-defined',
        '-D_WASI_EMULATED_SIGNAL', '-lwasi-emulated-signal',
        '-D_WASI_EMULATED_PROCESS_CLOCKS', '-lwasi-emulated-process-clocks',
        '-D_WASI_EMULATED_MMAN', '-lwasi-emulated-mman',
        '-D_WASI_EMULATED_GETPID', '-lwasi-emulated-getpid',
        '-DSPEC', '-DNDEBUG', '-DSMALL_MEMORY', '-DSPEC_AUTO_SUPPRESS_OPENMP', '-g', '-DSPEC_LP64',
        '-O3',
    ]
    print(f'flags = {flags}')
    sources = [str(file) for file in src_dir.glob('*.cpp')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
