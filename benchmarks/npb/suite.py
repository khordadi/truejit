from multiprocessing import cpu_count
from multiprocessing.pool import ThreadPool
from pathlib import Path
import subprocess
from Benchmarking import Workload, Benchmark, Suite

directory = Path(__file__).parent
src_dir = directory / 'src'

suite_name = 'NPB'

benchmark_list = {
    'bt': 'BT',
    'cg': 'CG',
    'ep': 'EP',
    'ft': 'FT',
    'is': 'IS',
    'lu': 'LU',
    'mg': 'MG',
    'sp': 'SP'
}

classes = [
    'S', 'W', 'A', 'B', 'C', 'D'
]


def suite(clazz):
    benchmarks = [Benchmark(name, directory / benchmark / f'{clazz.lower()}.wasm', [Workload()]) for benchmark, name in
                  benchmark_list.items()]
    return Suite(suite_name, benchmarks)


def install():
    if not src_dir.exists():
        subprocess.run(['git', 'clone', 'https://github.com/GMAP/NPB-CPP', src_dir], check=True)
        with open(src_dir / 'NPB-SER/config/make.def', 'w') as f:
            f.write(
                """
                CC = /opt/wasi-sdk/bin/clang++ -std=c++14
                CLINK   = /opt/wasi-sdk/bin/clang++ -std=c++14
                C_LIB  = -lm
                C_INC = -I../common
                CFLAGS  = -O3
                CLINKFLAGS = -O3
                UCC     = g++
                BINDIR  = ../bin
                RAND   = randdp
                WTIME  = wtime.cpp
                """
            )
    else:
        raise Exception('NPB already installed')


def build():
    for benchmark in benchmark_list.keys():
        (directory / benchmark).mkdir(parents=True, exist_ok=True)

    for clazz in classes:
        root = src_dir / 'NPB-SER'
        subprocess.run(['make', *[benchmark for benchmark in benchmark_list.keys()], f'CLASS={clazz}'],
                       cwd=root, check=True)
        for benchmark in benchmark_list.keys():
            src = root / 'bin' / f'{benchmark}.{clazz}'
            dest = directory / benchmark / f'{clazz.lower()}.wasm'
            subprocess.run(['mv', src, dest], check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)
    else:
        raise Exception('NPB not installed')


if __name__ == '__main__':
    install()
    build()
    uninstall()
