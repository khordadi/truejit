from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'espeak'
binary = directory / 'espeak.wasm'
workloads = [
    Workload(None, ['-f', directory / 'input.txt', '-s', '120', '-w', directory / 'output_file.wav'], [directory])]
espeak = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang++'
    flags = [
        '-D_WASI_EMULATED_SIGNAL', '-lwasi-emulated-signal',
        '-O0',
        '-fno-exceptions',
        '-DPATH_ESPEAK_DATA=\"./espeak-data\"',
    ]
    sources = [str(file) for file in src_dir.glob('*.cpp')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
