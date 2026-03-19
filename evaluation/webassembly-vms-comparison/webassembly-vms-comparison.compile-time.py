import json
import subprocess
from time import sleep
from benchmarks import *
from virtualmachines import *

REPETITIONS = 3


def vm_comparison():
    virtual_machines = [
        VirtualMachine("truejit",
                       ["/truejit/build/tools/vm-with-compiler", "--threads=single", "--compilation-mode=jit",
                        "--predictor=none", "--network-latency=0"]),
        VirtualMachine(
            "wasmtime-cranelift",
            [f"{HOME}/wasmtime/target/release/wasmtime", "run", "-C", "compiler=cranelift", "-C",
             "parallel-compilation=no", "-C",
             "cache=no"],
        )]
    for suite in [
        ffmpeg,
        jetstream,
        polybench,
        mibench,
        wabench,
        npb,
        coremark,
        spec,
    ]:
        suite_name = suite.mode
        base_dir = suite.base_dir
        benchmarks = suite.benchmarks

        for benchmark_name, args in benchmarks:
            binary = base_dir / benchmark_name / f"{benchmark_name}.wasm"
            run_in = str(base_dir / benchmark_name)
            stdin_file = None
            if "<" in args:
                stdin_file = base_dir / benchmark_name / args[args.i("<") + 1]
                args = args[: args.i("<")]

            for vm in virtual_machines:
                benchmark_args = args[:]
                command = []
                command += vm.command
                vm_name = vm.name
                match vm_name:
                    case vm_name if vm_name.startswith("wasmer"):
                        if benchmark_name in ["espeak", "mnist", "nab"]:
                            print(f"{suite_name},{benchmark_name},{vm_name},-1,-1")
                            continue
                    case vm_name if vm_name.startswith("graalwasm"):
                        if benchmark_name in ["espeak"]:
                            print(f"{suite_name},{benchmark_name},{vm_name},-1,-1")
                            continue

                match vm_name:
                    case vm_name if vm_name.startswith(("truejit", "wasmtime")):
                        command += [f"--dir=."]
                    case vm_name if vm_name.startswith("wasmer"):
                        command += [f"--dir={run_in}"]
                    case vm_name if vm_name.startswith("wazero"):
                        command += [f"-mount=."]
                    case vm_name if vm_name.startswith("wavm"):
                        command += ["--mount-root", run_in]
                    case vm_name if vm_name.startswith("graalwasm"):
                        command += [f"--wasm.WasiMapDirs=."]

                command += [str(binary)]

                match vm_name:
                    case vm_name if vm_name.startswith("wasmer"):
                        command += ["--"] if len(benchmark_args) > 0 else []
                        benchmark_args = [
                            run_in / arg if isinstance(arg, Path) else arg for arg in benchmark_args
                        ]

                command += [str(arg) for arg in benchmark_args]

                # print(f"[running] {suite_name}::{benchmark_name} @ {vm.name}")
                # print(f"cd {run_in}")
                command_str = " ".join(command)
                if stdin_file:
                    command_str = f"{command_str} < {stdin_file}"

                # print(command_str)

                try:
                    waitings = []
                    end_to_ends = []
                    for _ in range(REPETITIONS):
                        stdin = None
                        if stdin_file:
                            stdin = open(stdin_file, "r")
                        # command = ["/usr/bin/time", "-v"] +
                        # print(f"[running] {suite_name}::{benchmark_name} @ {vm.name}")
                        # print(f"cd {run_in}")
                        # print(command_str)
                        subprocess.run(
                            command,
                            cwd=run_in,
                            stdin=stdin,
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                            check=True,
                        )

                        with open("/tmp/result.waiting.txt", "r") as f:
                            # read the only line
                            line = f.readline()
                            # line is a flaot
                            waiting = float(line)
                            # print(f"waiting     {waiting:.03}")
                            waitings.append(waiting)
                        with open("/tmp/result.end-to-end.txt", "r") as f:
                            # read the only line
                            line = f.readline()
                            # line is a flaot
                            e2e = float(line)
                            # print(f"end-to-end  {e2e:.03}")
                            end_to_ends.append(e2e)
                    waitings = waitings[1:]
                    waitings_avg = sum(waitings) / len(waitings)
                    end_to_ends = end_to_ends[1:]
                    end_to_ends_avg = sum(end_to_ends) / len(end_to_ends)
                    print(f"{suite_name}::{benchmark_name} @ {vm_name} ({waitings_avg:.03f}, {end_to_ends_avg:.03f})")
                    # print()

                except subprocess.CalledProcessError as e:
                    print(f"[error] {e.stderr}")
                    print(f"{suite_name},{benchmark_name},{vm_name},-1,-1")



if __name__ == "__main__":
    vm_comparison()
