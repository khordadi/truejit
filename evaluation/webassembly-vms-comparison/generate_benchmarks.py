import json
from pathlib import Path
from benchmarks import *


def generate_json():
    benchmarks_json = []
    for suite in [
        polybench,
        jetstream,
        mibench,
        wabench,
        npb,
        coremark,
        spec
    ]:
        suite_name = suite.mode
        base_dir = suite.base_dir
        if suite_name == "polybench":
            base_dir = Path("/truejit/benchmarks/polybench/bin/mini")
        benchmarks = suite.benchmarks

        for benchmark_name, args in benchmarks:
            binary = base_dir / benchmark_name / f"{benchmark_name}.wasm"
            run_in = str(base_dir / benchmark_name)

            benchmark = {
                "name": benchmark_name,
                "suite": suite_name,
                "binary": str(binary),
                "args": [str(arg) for arg in args],
                "pre_opens": [run_in],
            }
            benchmarks_json.append(benchmark)

    print(json.dumps(benchmarks_json, indent=4))
    with open("/truejit/benchmarks/benchmarks.json", "w") as f:
        json.dump(benchmarks_json, f, indent=4)


if __name__ == '__main__':
    generate_json()
