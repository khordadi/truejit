from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

binary = directory / 'blowfish.wasm'
blowfish_decode = Benchmark('blowfish-decode', binary, [
    Workload('', ['d', 'input_large.enc', 'output_large.asc', '1234567890abcdeffedcba0987654321'],
             [directory / 'decode'])])
blowfish_encode = Benchmark('blowfish-encode', binary, [
    Workload('', ['e', 'input_large.asc', 'output_large.enc', '1234567890abcdeffedcba0987654321'],
             [directory / 'encode'])])


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-Wno-implicit-function-declaration', '-fomit-frame-pointer', '-O2']
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
