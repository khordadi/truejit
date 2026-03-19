from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'cjpeg'
binary = directory / 'cjpeg.wasm'
workloads = [
    Workload('', ['-dct', 'int', '-progressive', '-opt', '-outfile', 'output_large_encode.jpeg', 'input_large.ppm'],
             [directory])]
cjpeg = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O3', '-lm']
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
