from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'snappy'
binary = directory / 'snappy.wasm'
workloads = [Workload()]
snappy = Benchmark(name, binary, workloads)


def build():
    # /opt/wasi-sdk/bin/clang++ -D_FILE_OFFSET_BITS=64 -fno-exceptions -O2 -o snappy.wasm ./src/*.cc
    compiler = '/opt/wasi-sdk/bin/clang++'
    flags = [
        '-O2',
        '-fno-exceptions',
        '-D_FILE_OFFSET_BITS=64'
    ]
    sources = [str(file) for file in src_dir.glob('*.cc')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
