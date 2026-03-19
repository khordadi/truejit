import subprocess
import json
import time
from concurrent.futures import ThreadPoolExecutor

llvm_versions = range(9, 19)
llvm_root = '/tmp/llvm'
uftrace_script = '/truejit/evaluation/uftrace/uftrace_script.py'
postprocessing_script = '/truejit/evaluation/uftrace/postprocessing.py'
sqlite3_src_dir = '/tmp/llvm/sqlite-amalgamation-3450300'
benchmarking_dir = '/truejit/evaluation/llvm-versions-comparison'


class Command:
    def __init__(self, name, binary, args, dir):
        self.name = name
        self.binary = binary
        self.args = args
        self.dir = dir

    def __str__(self):
        return f"cd {self.dir}\n{self.binary} {' '.join(self.args)}"


def generate_sequence(command):
    print(f"Running version {command.binary} {command.args} in {command.pre_opens}")
    # record_command = ["uftrace", "record", "--nest-libcall",
    #                   f"--script={uftrace_script}",
    #                   f"{llvm_root}/llvm-{version}/bin/clang", "-O3",
    #                   f"{sqlite3_src_dir}/shell.c",
    #                   f"{sqlite3_src_dir}/sqlite3.c",
    #                   "-lpthread", "-ldl", "-o", "sqlite3"]

    uftrace_command = ["uftrace", "record", "--nest-libcall",
                       f"--script={uftrace_script}"]
    record_command = uftrace_command + [command.binary] + command.args
    with subprocess.Popen(record_command, cwd=command.pre_opens, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as uftrace:
        postprocessing_command = ['python3', postprocessing_script]
        with subprocess.Popen(postprocessing_command, stdin=uftrace.stderr,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE) as postprocessing:
            _, output = postprocessing.communicate()
            output = output.decode('utf-8')

            with open(f'sequences/{command.mode}.json', 'w') as f:
                f.write(output)

    # remove uftrace.profiling and uftrace.profiling.old directories
    subprocess.run(["rm", "-rf", "uftrace.profiling", "uftrace.profiling.old"], cwd=command.pre_opens)


def main():
    # rm -rf sequences
    subprocess.run(["rm", "-rf", "sequences"])
    # mkdir sequences
    subprocess.run(["mkdir", "sequences"])

    commands = []
    for version in llvm_versions:
        binary = f"{llvm_root}/llvm-{version}/bin/clang"
        args = ["-O3",
                f"{sqlite3_src_dir}/shell.c",
                f"{sqlite3_src_dir}/sqlite3.c",
                "-lpthread", "-ldl", "-o", "sqlite3"]
        # args = ["-O0",
        #         f"{benchmarking_dir}/empty.c", "-o", f"{benchmarking_dir}/empty"]
        dir = f"/tmp/uftrace-llvm-{version:2}-{int(time.time())}"
        subprocess.run(["mkdir", "-p", dir])

        commands.append(Command(f"llvm-{version}", binary, args, dir))

    # Use a ThreadPoolExecutor to run each version in parallel
    with ThreadPoolExecutor(max_workers=len(commands)) as executor:
        executor.map(generate_sequence, commands)


if __name__ == '__main__':
    main()
