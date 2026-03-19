import os
import subprocess
import json
import time
from pathlib import Path
import multiprocessing
from time import gmtime, strftime

start_time = time.time()


def now():
    return strftime('%H:%M:%S', gmtime(time.time() - start_time))


root_dir = Path(__file__).parent.parent
gdb_script = root_dir / 'trace' / 'gdb' / 'gdb.py'
gdb_command = ["gdb", "--batch-silent",
               f"--command={gdb_script}",
               "--args"]
trace_dir = root_dir / 'trace'
sequences_dir = trace_dir / 'sequences'


class Command:
    def __init__(self, id, binary, args, cwd, benchmark):
        self.id = id
        self.binary = binary
        self.args = args
        self.cwd = cwd
        self.benchmark = benchmark

    def __str__(self):
        return f"cd {self.cwd} && {self.binary} {' '.join(self.args)}"


def trace(command):
    # print(f"[{now()}][run-start:{command.id}] {command}")
    # try:
    #     subprocess.run([command.binary, *command.args], cwd=command.cwd, capture_output=True, text=True, check=True)
    # except subprocess.CalledProcessError as e:
    #     print(f"[{now()}][run-error:{command.id}] {command}\n{e.stderr}")
    #     return
    # print(f"[{now()}][run-end:{command.id}] {command}")

    try:
        print(f"[{now()}][trace-start:{command.id}] {command}")
        process = subprocess.run([*gdb_command, command.binary, *command.args], cwd=command.cwd, capture_output=True,
                                 text=True, check=True)
        print(f"[{now()}][trace-end:{command.id}] {command}")
    except subprocess.CalledProcessError as e:
        print(f"[{now()}][trace-error:{command.id}] {command}\n{e.stderr}")
        return

    try:
        stderr = process.stderr
        trace_output = stderr.splitlines()[-1]  # trace is the last line of stderr
        sequence = json.loads(trace_output)
        json_file = root_dir / 'trace' / 'sequences' / f'{command.benchmark}.json'
        with open(json_file, 'a') as f:
            f.write(json.dumps(sequence) + '\n')
    except Exception as e:
        print(f"[{now()}][json-error:{command.id}] {command}\n{e}")
        print(f"stdout: {process.stdout}")
        print(f"stderr: {stderr}")
        return


spec_benchmarks_root = '/tmp/spec/benchspec/CPU'
spec_bin_label = 'truejit-m64'

benchmarks = [
    ("502.gcc_r", f'{spec_benchmarks_root}/502.gcc_r/exe/cpugcc_r_base.{spec_bin_label}'),
    ("505.mcf_r", f'{spec_benchmarks_root}/505.mcf_r/exe/mcf_r_base.{spec_bin_label}'),
    ("507.cactuBSSN_r", f'{spec_benchmarks_root}/507.cactuBSSN_r/exe/cactusBSSN_r_base.{spec_bin_label}'),
    ("511.povray_r", f'{spec_benchmarks_root}/511.povray_r/exe/povray_r_base.{spec_bin_label}'),
    ("519.lbm_r", f'{spec_benchmarks_root}/519.lbm_r/exe/lbm_r_base.{spec_bin_label}'),
    ("523.xalancbmk_r", f'{spec_benchmarks_root}/523.xalancbmk_r/exe/cpuxalan_r_base.{spec_bin_label}'),
    ("525.x264_r", f'{spec_benchmarks_root}/525.x264_r/exe/x264_r_base.{spec_bin_label}'),
    ("531.deepsjeng_r", f'{spec_benchmarks_root}/531.deepsjeng_r/exe/deepsjeng_r_base.{spec_bin_label}'),
    ("541.leela_r", f'{spec_benchmarks_root}/541.leela_r/exe/leela_r_base.{spec_bin_label}'),
    ("544.nab_r", f'{spec_benchmarks_root}/544.nab_r/exe/nab_r_base.{spec_bin_label}'),
    ("548.exchange2_r", f'{spec_benchmarks_root}/548.exchange2_r/exe/exchange2_r_base.{spec_bin_label}'),
    ("557.xz_r", f'{spec_benchmarks_root}/557.xz_r/exe/xz_r_base.{spec_bin_label}'),
]


def main():
    benchmarks_dir = root_dir / 'benchmarks'
    print(f"benchmarks_dir: {benchmarks_dir}")
    workloads_name = 'alberta'
    workloads_dir = benchmarks_dir / workloads_name
    print(f"workloads_dir: {workloads_dir}")

    # make sure sequences_dir exists use Path
    print(f"remove existing sequences_dir: {sequences_dir}")
    sequences_dir.mkdir(parents=True, exist_ok=True)

    print(f"processing workloads in {workloads_dir}...")
    commands = []
    for benchmark_name, benchmark_binary in benchmarks:
        json_file = sequences_dir / f'{benchmark_name}.json'
        # remove existing json file using Path
        json_file.unlink(missing_ok=True)

        benchmark_dirs = [workloads_dir / benchmark_name, Path(f'/tmp/spec/benchspec/CPU/{benchmark_name}/data')]
        for benchmark_dir in benchmark_dirs:
            print(f"benchmark_dir: {benchmark_dir}")

            for directory in os.listdir(benchmark_dir):
                if os.path.isdir(benchmark_dir / directory):
                    print(f"[workload] {directory}")
                    # check if 'input' dir exists
                    input_dir = benchmark_dir / directory / 'input'
                    if not os.path.exists(input_dir):
                        print(f"input dir does not exist")
                        continue

                    # check if control file exists
                    control_file = input_dir / 'control'
                    if not os.path.exists(control_file):
                        print(f"control file does not exist")
                        continue

                    # open control file
                    with open(control_file, 'r') as f:
                        # for each args
                        for args in f:
                            args = args.strip()
                            # if empty or start with #
                            if not args or args.startswith('#'):
                                continue
                            args = args.split()

                            command = Command(len(commands), benchmark_binary, args, input_dir, benchmark_name)
                            # command = Command(len(commands), "/bin/ls", [], input_dir, benchmark_name)  # for testing
                            print(f"[cmd#{command.id}] {command}")
                            commands.append(command)

    print(f"[total-commands] {len(commands)}")

    cores = os.cpu_count()
    with multiprocessing.Pool(cores) as pool:
        pool.map(trace, commands)


if __name__ == '__main__':
    main()
