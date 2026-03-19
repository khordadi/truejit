import json
import multiprocessing
import subprocess
from pathlib import Path


class CompilationServiceProfile:
    def __init__(self, j):
        self.times = j['times']
        self.total = j['total']

    def __repr__(self):
        return f"CompilationService({self.total}, {self.times})"


class VirtualMachineProfile:
    def __init__(self, j):
        self.start = j['start']
        self.end = j['end']
        self.init_start = j['init-start']
        self.init_end = j['init-end']
        self.static_start = j['static-start']
        self.static_end = j['static-end']
        self.times = j['times']
        self.waiting = j['waiting']
        self.e2e = j['e2e']
        self.exec = j['exec']

    def __repr__(self):
        return f"VirtualMachine({self.start}, {self.end}, {self.init_start}, {self.init_end}, {self.static_start}, {self.static_end}, {self.times}, {self.waiting}, {self.e2e}, {self.exec})"


class WasmServiceProfile:
    def __init__(self, j):
        self.times = j['times']
        self.total = j['total']

    def __repr__(self):
        return f"WasmService({self.total}, {self.times})"


class SignatureServiceProfile:
    def __init__(self, j):
        self.total = j['total']

    def __repr__(self):
        return f"SignatureService({self.total})"


class PredictionServiceProfile:
    def __init__(self, j):
        self.total = j['total']

    def __repr__(self):
        return f"PredictionService({self.total})"


class Profile:
    def __init__(self, j):
        self.title = j['title']
        self.start = j['start']
        self.end = j['end']
        if 'compilation-service' in j:
            self.compilation_service = CompilationServiceProfile(j['compilation-service'])
        if 'virtual-machine' in j:
            self.virtual_machine = VirtualMachineProfile(j['virtual-machine'])
        if 'wasm-service' in j:
            self.wasm_service = WasmServiceProfile(j['wasm-service'])
        if 'signature-service' in j:
            self.signature_service = SignatureServiceProfile(j['signature-service'])
        else:
            self.signature_service = None
        if 'prediction-service' in j:
            self.prediction_service = PredictionServiceProfile(j['prediction-service'])
        else:
            self.prediction_service = None


# --------------------------------
# tools
# --------------------------------
cmake_bin = '/usr/local/bin/cmake'
cmake_args = ['-DCMAKE_BUILD_TYPE=Release', '-GNinja', '-S /truejit']
ncores = multiprocessing.cpu_count()


class StaticAsyncPlanner:
    def __init__(self):
        dir = Path('/truejit/StaticAsyncPlanner/')
        if not dir.exists():
            dir.mkdir(parents=True, exist_ok=True)
            subprocess.run([cmake_bin] + cmake_args, cwd=dir, check=True, capture_output=True)
        subprocess.run([cmake_bin, '--build', str(dir), '--target', 'static-planner', '-j', str(ncores)], check=True,
                       capture_output=True)
        self.bin = dir / 'tools/static-planner'

    def generate(self, benchmark):
        plan_path = Path(str(benchmark.binary).replace('benchmarks', 'plans'))
        plan_path = plan_path.with_suffix('.json')
        plan_path.parent.mkdir(parents=True, exist_ok=True)

        cmd = [self.bin, '-i', benchmark.binary, '-o', plan_path]
        subprocess.run(cmd, check=True)


PROFILE_FLAGS = {
    'specialization': 'PROFILE_FOR_SPECIALIZATION',
    'calls': 'PROFILE_CALLS',
    'compilation-service': 'PROFILE_COMPILATION_SERVICE',
    'virtual-machine': 'PROFILE_VIRTUAL_MACHINE',
    'wasm-service': 'PROFILE_WASM_SERVICE',
    'signature-service': 'PROFILE_SIGNATURE_SERVICE',
    'prediction-service': 'PROFILE_PREDICTION_SERVICE',
}


class StaticCompiler:
    def __init__(self, profiling=None):
        if profiling is None:
            profiling = ['compilation-service']
        p_str = '_'.join(profiling)
        dir = Path('/truejit/compiler/') / p_str
        if not dir.exists():
            dir.mkdir(parents=True, exist_ok=True)
            profiling = [f'-D{PROFILE_FLAGS[flag]}=On' for flag in profiling]
            subprocess.run([cmake_bin] + cmake_args + profiling, cwd=dir, check=True)
        subprocess.run([cmake_bin, '--build', str(dir), '--target', 'static-compiler', '-j', str(ncores)], check=True,
                       capture_output=True)
        self.bin = dir / 'tools/static-compiler'

    def compile(self, benchmark, options):
        profile = Path('/tmp/static-compiler.json')
        subprocess.run([self.bin] + options + [benchmark.binary],
                       env={'PROFILE': str(profile)},
                       check=True)
        j = json.load(open(profile, 'r'))
        profile.unlink()
        return CompilationServiceProfile(j['compilation-service'])


class VirtualMachine:
    def __init__(self, profiling=None):
        if profiling is None:
            profiling = [
                'compilation-service',
                'virtual-machine',
                'wasm-service',
                'signature-service',
                'prediction-service'
            ]
        # profiling to string; e.g., 'compilation-service_virtual-machine_wasm-service'
        p_str = '_'.join(profiling)
        dir = Path(f'/tmp/vms/') / p_str
        if not dir.exists():
            dir.mkdir(parents=True, exist_ok=True)
            profiling = [f'-D{PROFILE_FLAGS[flag]}=On' for flag in profiling]
            subprocess.run([cmake_bin] + cmake_args + profiling, cwd=dir, check=True, capture_output=True)
        subprocess.run([cmake_bin, '--build', str(dir), '--target', 'vm-with-compiler', '-j', str(ncores)], cwd=dir,
                       check=True, capture_output=True)
        self.bin = dir / 'tools/vm-with-compiler'

    def run(self, benchmark, vm_opts=None):
        if vm_opts is None:
            vm_opts = []
        pre_opens = [f'--dir={dir}' for dir in benchmark.workloads[0].pre_opens]
        args = benchmark.workloads[0].args
        env = {'PROFILE': '/tmp/vm.json'}
        if len(vm_opts) == 0:
            history_path = Path(str(benchmark.binary).replace('benchmarks', 'histories'))
            history_path = history_path.with_suffix('.json')
            history_path.parent.mkdir(parents=True, exist_ok=True)
            env['HISTORY'] = str(history_path)
        cmd = [self.bin] + vm_opts + pre_opens + [benchmark.binary] + args
        # print cmd as a string
        # print(' '.join([str(o) for o in cmd]))
        out = subprocess.run(cmd, env=env, capture_output=True)
        # if not successful, print the output
        if out.returncode != 0:
            print(out.stderr.decode('utf-8'))
            raise Exception('VM failed')

        # print only the last line of the output
        out = out.stdout.decode('utf-8').splitlines()
        if 'virtual-machine' in out[-2]:
            print(out[-2])
            print('\033[0m', end='')  # reset color code
        j = json.load(open('/tmp/vm.json', 'r'))
        return Profile(j)


class SpecializationProfiler:
    def __init__(self):
        self.vm = VirtualMachine(['specialization'])

    def profile(self, benchmark):
        self.vm.run(benchmark)
        candidates = json.load(open('/tmp/specs.json'))
        candidates = {int(o['idx']): o['name'] for o in candidates}  # {fn_idx: fn_name}
        report = json.load(open('/tmp/report.json'))
        metrics = {}  # {fn_idx: {params, calls, size_before, size_after}}
        for entry in report:
            metrics[int(entry['idx'])] = entry
            del entry['idx']
            del entry['duration']
            del entry['name']
        return candidates, metrics


class CompilerProfiler:
    def __init__(self):
        self.vm = VirtualMachine(['compilation-service'])

    def profile(self, benchmark, opts=None):
        prof = self.vm.run(benchmark, opts)
        prof = prof.compilation_service.times
        prof = {fn: end - start for [fn, size, start, end] in prof}
        return prof


class CallProfiler:
    def __init__(self):
        self.vm = VirtualMachine(['calls'])

    def profile(self, benchmark, opts=None):
        prof = self.vm.run(benchmark, opts)
        prof = json.loads(open('/tmp/calls.json', 'r').read())
        prof = {int(k): v for k, v in prof.items()}  # {fn_idx: duration}
        return prof


if __name__ == '__main__':
    vm = VirtualMachine()
    print(vm)
