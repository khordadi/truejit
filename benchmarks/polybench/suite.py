from multiprocessing import cpu_count
from multiprocessing.pool import ThreadPool
from pathlib import Path
import subprocess
from Benchmarking import Workload, Benchmark, Suite

directory = Path(__file__).parent
src_dir = directory / 'src'

suite_name = 'PolyBench'

benchmark_list = directory / 'benchmark_list'


def suite(dataset):
    benchmark_files = [src_dir / benchmark for benchmark in benchmark_list.read_text().splitlines()]
    benchmarks = [benchmark.stem for benchmark in benchmark_files]
    benchmarks = [Benchmark(benchmark, directory / benchmark / f'{dataset}.wasm', [Workload()]) for benchmark in
                  benchmarks]
    return Suite(suite_name, benchmarks)


def install():
    if not src_dir.exists():
        subprocess.run(
            ['git', 'clone', 'https://github.com/MatthiasJReisinger/PolyBenchC-4.2.1.git', src_dir],
            check=True)
        subprocess.run(['cp', src_dir / 'utilities/benchmark_list', benchmark_list], check=True)
    else:
        raise Exception('PolyBench already installed')


def build_suite(dataset):
    benchmark_files = [src_dir / benchmark for benchmark in benchmark_list.read_text().splitlines()]
    results = ThreadPool(cpu_count()).imap_unordered(build_benchmark_for_dataset(dataset), benchmark_files)
    for _ in results:
        pass


def build_benchmark_for_dataset(dataset):
    return lambda benchmark_file: build_benchmark(benchmark_file, dataset)


datasets = {
    'mini': 'MINI_DATASET',
    'small': 'SMALL_DATASET',
    'medium': 'MEDIUM_DATASET',
    'standard': 'STANDARD_DATASET',
    'large': 'LARGE_DATASET',
    'extra-large': 'EXTRALARGE_DATASET'
}


def build_benchmark(benchmark_file, dataset):
    benchmark_dir = benchmark_file.parent
    benchmark_name = benchmark_file.stem
    bin_dir = directory / benchmark_name
    bin_dir.mkdir(parents=True, exist_ok=True)

    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O3',
             '-Wl,--export=__heap_base',
             '-Wl,--export=__data_end',
             '-Wl,--export=malloc',
             '-Wl,--export=free',
             f'-I{src_dir / 'utilities'}',
             f'-I{benchmark_dir}',
             '-DPOLYBENCH_TIME',
             '-D_WASI_EMULATED_PROCESS_CLOCKS',
             f'-D{datasets[dataset]}'
             ]
    sources = [src_dir / 'utilities/polybench.c', benchmark_file]
    subprocess.run([compiler] + flags + sources + ['-o', bin_dir / f'{dataset}.wasm'], check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)
    else:
        raise Exception('PolyBench not installed')


if __name__ == '__main__':
    install()
    [build_suite(dataset) for dataset in datasets.keys()]
    uninstall()
