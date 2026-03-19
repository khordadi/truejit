from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'gcc-loops'
binary = directory / 'gcc-loops.wasm'
workloads = [Workload('default', [])]
gcc_loops = Benchmark(name, binary, workloads)


def install():
    if not src_dir.exists():
        src_dir.mkdir()
        subprocess.run(['wget', '-N', 'https://browserbench.org/JetStream/wasm/gcc-loops.cpp'], check=True, cwd=src_dir)


def build():
    # native
    compiler = 'clang++'
    flags = ['-O3', '-fno-exceptions']
    sources = ['gcc-loops.cpp']
    subprocess.run([compiler, *flags, *sources, '-o', binary.with_suffix('.native')], cwd=src_dir, check=True)

    # wasm
    compiler = '/opt/wasi-sdk/bin/clang++'
    flags = ['-O3',
             '-z', 'stack-size=32768',
             '-Wl,--initial-memory=1048576',
             '-Wl,--export=main',
             '-Wl,--export=__heap_base'
             '-Wl,--export=__data_end',
             '-Wl,--allow-undefined',
             '-Wno-implicit-function-declaration',
             '-fno-exceptions']
    sources = ['gcc-loops.cpp']
    subprocess.run([compiler, *flags, *sources, '-o', binary], cwd=src_dir, check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)


if __name__ == '__main__':
    src_dir = directory / 'src'

    install()
    build()
    uninstall()
