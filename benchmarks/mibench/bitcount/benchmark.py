from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'bitcount'
binary = directory / 'bitcount.wasm'
workloads = [Workload('', ['1125000'])]
bitcount = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-D_WASI_EMULATED_PROCESS_CLOCKS', '-O3', '-lm', '-lwasi-emulated-process-clocks']
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
