from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'stringsearch'
binary = directory / 'stringsearch.wasm'
workloads = [Workload()]
stringsearch = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-std=c89', '-O2']
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
