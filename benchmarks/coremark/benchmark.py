from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'CoreMark'
binary = directory / 'coremark.wasm'
workloads = [Workload()]
benchmark = Benchmark(name, binary, workloads)


def install():
    if not src_dir.exists():
        subprocess.run(['git', 'clone', 'https://github.com/eembc/coremark.git', src_dir], check=True)


def build():
    # native
    compiler = 'clang'
    flags = ['-O2',
             '-Iposix', '-I.',
             '-DITERATIONS=300000',
             '-DSEED_METHOD=SEED_VOLATILE',
             '-DPERFORMANCE_RUN=1',
             '-DFLAGS_STR="-O2 -DITERATIONS=300000 -DSEED_METHOD=SEED_VOLATILE -DPERFORMANCE_RUN=1"']
    sources = ['core_list_join.c',
               'core_main.c',
               'core_matrix.c',
               'core_state.c',
               'core_util.c',
               'posix/core_portme.c']
    subprocess.run([compiler] + flags + sources + ['-o', binary.with_suffix('.native')], cwd=src_dir, check=True)

    # wasm
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O2',
             '-Iposix', '-I.',
             '-Wl,--export=main', '-Wl,--allow-undefined',
             '-DITERATIONS=300000',
             '-DSEED_METHOD=SEED_VOLATILE',
             '-DPERFORMANCE_RUN=1',
             '-DFLAGS_STR="-O2 -DITERATIONS=300000 -DSEED_METHOD=SEED_VOLATILE -DPERFORMANCE_RUN=1"']
    sources = ['core_list_join.c',
               'core_main.c',
               'core_matrix.c',
               'core_state.c',
               'core_util.c',
               'posix/core_portme.c']
    subprocess.run([compiler] + flags + sources + ['-o', binary], cwd=src_dir, check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)


if __name__ == '__main__':
    src_dir = directory / 'src'

    install()
    build()
    uninstall()
