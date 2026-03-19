from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'mnist'
binary = directory / 'mnist.wasm'
workloads = [Workload('', [], [directory])]
mnist = Benchmark(name, binary, workloads)


def build():
    # /opt/wasi-sdk/bin/clang -O2 -I. -o mnist.wasm ./src/*.c
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = [
        '-O2',
        '-I.',
        '-D_WASI_EMULATED_SIGNAL', '-lwasi-emulated-signal'
    ]
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
