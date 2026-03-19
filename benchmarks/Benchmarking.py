import subprocess
from pathlib import Path


class Workload:
    def __init__(self, name=None, args=None, pre_opens=None):
        self.name = name
        self.args = args if args is not None else []
        self.pre_opens = pre_opens if pre_opens is not None else []

    def __str__(self):
        return f'Workload(name={self.name}, args={self.args}, pre_opens={self.pre_opens})'


class Benchmark:
    def __init__(self, name, binary, workloads):
        self.name = name
        self.binary = binary
        self.workloads = workloads

    def __str__(self):
        return f'Benchmark(name={self.name}, binary={self.binary}, workloads={self.workloads})'


class Suite:
    def __init__(self, name, benchmarks):
        self.name = name
        self.benchmarks = benchmarks

    def __str__(self):
        return f'Suite(name={self.name}, benchmarks={self.benchmarks})'


class VirtualMachine:
    def __init__(self, name, binary):
        self.name = name
        self.binary = binary

    def run(self, app, workload, vm_options, env=None):
        cwd = workload.pre_opens[0] if workload.pre_opens else Path(app).parent
        opts = vm_options[:]
        opts += [f'--dir={pre_open}' for pre_open in workload.pre_opens]
        env = env if env is not None else {}

        env_str = ' '.join([f'{k}={v}' for k, v in env.items()]) + ' ' if env else ''
        vm_options_str = ' '.join(opts) + ' ' if opts else ''
        args = [str(arg) if isinstance(arg, Path) else arg for arg in workload.args]
        workload_str = ' '.join(args)

        print(f'cd {cwd}')
        print(f'{env_str}{self.binary} {vm_options_str}{app} {workload_str}')
        out = subprocess.run([self.binary] + opts + [app] + args, env=env, cwd=cwd, check=True, capture_output=True)
        out = out.stdout.decode('utf-8')
        # print the last line
        print(out.split('\n')[-2])

    def __str__(self):
        return f'VirtualMachine(name={self.name}, binary={self.binary})'


class Wasmtime(VirtualMachine):
    def __init__(self):
        super().__init__('wasmtime', '/users/khordadi/.wasmtime/bin/wasmtime')


class TrueJIT(VirtualMachine):
    def __init__(self):
        # super().__init__('truejit', '/truejit/debug/tools/vm-with-compiler')
        super().__init__('truejit', '/truejit/release/tools/vm-with-compiler')
