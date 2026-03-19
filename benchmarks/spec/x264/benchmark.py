from pathlib import Path
import subprocess

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'x264'
binary = directory / 'x264.wasm'
workloads = [Workload(name=None,
                      args=['--dumpyuv', '50', '--frames', '142', '-o', directory / 'in.264', directory / 'in.yuv',
                            '1280x720'], pre_opens=[directory])]
x264 = Benchmark(name, binary, workloads)


def build():
    raise NotImplementedError(f'build() for `{name}` not implemented')
