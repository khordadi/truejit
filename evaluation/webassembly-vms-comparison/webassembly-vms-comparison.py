import subprocess
from time import sleep
from benchmarks import *
from virtualmachines import *

REPETITIONS = 3


def vm_comparison():
    remote_compiler_command = [
        "/truejit/build/tools/remote-compiler",
        "--ip=0.0.0.0",
        "--threads=multi",
        "--compilation-mode=truejit",
        "--predictor=100-gram-lstm",
        "--network-latency=0",
    ]
    print(f"[running-compiler] {' '.join(remote_compiler_command)}")
    remote_compiler_process = subprocess.Popen(
        remote_compiler_command, stdout=subprocess.DEVNULL
    )
    sleep(10)
    for suite in [
        jetstream,
        polybench,
        mibench,
        wabench,
        npb,
        coremark,
        spec
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
                vm_name = vm.mode
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
                    times = []
                    memories = []
                    for _ in range(REPETITIONS):
                        stdin = None
                        if stdin_file:
                            stdin = open(stdin_file, "r")
                        command = ["/usr/bin/time", "-v"] + command
                        out = subprocess.run(
                            command,
                            cwd=run_in,
                            stdin=stdin,
                            check=True,
                            capture_output=True,
                        )
                        stderr = out.stderr.decode("utf-8")
                        stderr = stderr.split("\n")

                        memory = None
                        for line in stderr:
                            if "Maximum resident set size (kbytes):" in line:
                                memory = int(line[len("\tMaximum resident set size (kbytes): "):])
                                break

                        clock_time = None
                        for line in stderr:
                            if "Elapsed (wall clock) time (h:mm:ss or m:ss):" in line:
                                clock_time = line[len("\tElapsed (wall clock) time (h:mm:ss or m:ss): "):]
                                break
                        clock_time = clock_time.split(":")
                        minutes = int(clock_time[0])
                        seconds = float(clock_time[1])
                        clock_time = minutes * 60 + seconds

                        times.append(clock_time)
                        memories.append(memory)
                    if len(times) > 1 and len(memories) > 1:
                        times = times[1:]
                        memories = memories[1:]
                    avg_time = sum(times) / len(times)
                    avg_memory = sum(memories) / len(memories)
                    print(f"{suite_name},{benchmark_name},{vm_name},{avg_time:.04f},{int(avg_memory) // 1000:d}")
                except subprocess.CalledProcessError as e:
                    print(f"[error] {e.stderr}")
                    print(f"{suite_name},{benchmark_name},{vm_name},-1,-1")
    remote_compiler_process.terminate()


if __name__ == "__main__":
    vm_comparison()
