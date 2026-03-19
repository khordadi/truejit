from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'gnuchess'
binary = directory / 'gnuchess.wasm'
workloads = [Workload(None, ['<', directory / 'input'], [])]
gnuchess = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = [
        '-O2',
        '-I.',
        '-D_WASI_EMULATED_SIGNAL', '-lwasi-emulated-signal',
    ]
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
