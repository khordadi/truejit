from pathlib import Path
import subprocess

from nltk import download

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'SQLite'
binary = directory / 'sqlite.wasm'
workloads = [Workload(None, ['<', directory / 'workloads/0' / 'in.sql'], pre_opens=[directory / 'workloads/0'])]
sqlite = Benchmark(name, binary, workloads)


def install():
    if not src_dir.exists():
        subprocess.run(['wget', 'https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip', '-O', 'sqlite.zip'],
                       check=True)
        subprocess.run(['unzip', 'sqlite.zip'], check=True)
        subprocess.run(['rm', 'sqlite.zip'], check=True)
        subprocess.run(['mv', 'sqlite-amalgamation-3460000', 'src'], check=True)
        subprocess.run(['sed', '-i', 's/system(zCmd)/0/g', 'src/shell.c'], check=True)  # Disable system() calls


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-D_WASI_EMULATED_GETPID', '-lwasi-emulated-getpid',
             '-D_WASI_EMULATED_SIGNAL', '-lwasi-emulated-signal',
             '-D_WASI_EMULATED_PROCESS_CLOCKS', '-lwasi-emulated-process-clocks',
             '-lpthread', '-ldl', '-lm']
    sources = ['shell.c', 'sqlite3.c']
    subprocess.run([compiler] + flags + sources + ['-o', binary], cwd=src_dir, check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)


if __name__ == '__main__':
    src_dir = directory / 'src'

    install()
    build()
    uninstall()
