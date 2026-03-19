from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'djpeg'
binary = directory / 'djpeg.wasm'
# $TRUEJIT --dir=. ./djpeg.wasm -dct int -ppm -outfile output_large_decode.ppm input_large.jpg
workloads = [
    Workload('', ['-dct', 'int', '-ppm', '-outfile', 'output_large_decode.ppm', 'input_large.jpg'],
             [directory])]
djpeg = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O3', '-lm']
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
