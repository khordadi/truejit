from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'bzip2'
binary = directory / 'bzip2.wasm'
# $TRUEJIT --dir=. ./bzip2.wasm -k -f -z bzip2.wasm
workloads = [Workload(None, ['-k', '-f', '-z', directory / 'bzip2.wasm'], [directory])]
bzip2 = Benchmark(name, binary, workloads)


def build():
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = [
        '-O0',
        '-D_WASI_EMULATED_SIGNAL', '-lwasi-emulated-signal',
        '-D_WASI_EMULATED_PROCESS_CLOCKS', '-lwasi-emulated-process-clocks',
        '-D_FILE_OFFSET_BITS=64',
    ]
    sources = [str(file) for file in src_dir.glob('*.c')]
    subprocess.run([compiler] + flags + ['-o', binary] + sources, check=True, cwd=src_dir)


if __name__ == '__main__':
    src_dir = directory / 'src'

    build()
