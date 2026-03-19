from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

binary = directory / 'rijndael.wasm'

rijndael_decode = Benchmark('rijndael-decode', binary, [
    Workload('', [directory / 'decode/input_large.enc', directory / 'decode/output_large.dec', 'd',
                  '1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321'],
             [directory / 'decode'])])
rijndael_encode = Benchmark('rijndael-encode', binary, [
    Workload('', [directory / 'encode/input_large.asc', directory / 'encode/output_large.enc', 'e',
                  '1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321'],
             [directory / 'encode'])])


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O2']
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
