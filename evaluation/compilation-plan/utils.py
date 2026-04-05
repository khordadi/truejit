import hashlib
import shutil
import subprocess
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import time

PROJECT_ROOT = Path('/users/khordadi/truejit')


def metric(path):
    if path.exists():
        return path.read_text().strip()
    else:
        raise RuntimeError(f"Metric file {path} does not exist")


def mv_profile(dst):
    get_dst = lambda: Path(dst) / datetime.now(ZoneInfo("Europe/London")).strftime("%Y-%m-%d_%H-%M-%S")
    dst = get_dst()
    while dst.exists():
        time.sleep(1)
        dst = get_dst()
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.move('/tmp/metric', str(dst))


def static_compile(wasm_file):
    static_compiler = PROJECT_ROOT / 'release/tools/static-compiler'
    if not static_compiler.exists():
        raise RuntimeError(f"static-compiler not found at {static_compiler}")
    subprocess.run([str(static_compiler), str(wasm_file)], check=True)


class Workload:
    def __init__(self, name, args, pre_opens):
        self.name = name
        self.args = args
        self.pre_opens = pre_opens

    def __str__(self):
        args = [str(arg) for arg in self.args]
        return f"[{self.name}] {' '.join(args)}"


class VirtualMachine:
    def __init__(self, build_flags=None):
        if build_flags is None:
            build_flags = []
        build_flags = [f'-D{flag}' for flag in build_flags]
        build_flags = ' '.join(build_flags)
        build_dir = Path(f'/tmp/vms/{abs(int(hashlib.sha256(build_flags.encode()).hexdigest(), 16))}')
        build_dir.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ['cmake', '-DCMAKE_BUILD_TYPE=Release', '-GNinja', '-S', str(PROJECT_ROOT), '-B', str(build_dir),
             build_flags],
            check=True)
        subprocess.run(['cmake', '--build', str(build_dir), '--target', 'vm-with-compiler', '-j64'], check=True)
        self.binary = f'{build_dir}/tools/vm-with-compiler'

    def run(self, binary, workload, options=None, env=None):
        if options is None:
            options = []
        if env is None:
            env = {}
        cwd = workload.pre_opens[0] if len(workload.pre_opens) > 0 else Path(binary).parent
        options += [f'--dir={str(p)}' for p in workload.pre_opens]

        cmd = [self.binary] + options + ['--'] + [binary] + workload.args
        print(' '.join([str(c) for c in cmd]))
        subprocess.run(cmd, check=True, cwd=cwd, env=env)


class Benchmark:
    def __init__(self, name, binary, workloads):
        self.name = name
        self.binary = binary
        self.workloads = workloads


wls = PROJECT_ROOT / 'benchmarks/ffmpeg/workloads'
ffmpeg = Benchmark('ffmpeg', PROJECT_ROOT / 'benchmarks/ffmpeg/ffmpeg.wasm',
                   [
                       Workload('mov-to-mp4', ['-y', '-i', wls / '0' / 'in.mov', wls / '0' / 'out.mp4'],
                                [wls / '0']),
                       Workload('mp4-to-mkv',
                                ['-y', '-i', wls / '1' / 'in.mp4', '-c:v', 'libx264', wls / '1' / 'out.mkv'],
                                [wls / '1']),
                       Workload('mp4-to-wav',
                                ['-y', '-i', wls / '2' / 'in.mp4', '-c:a', 'pcm_s16le', wls / '2' / 'out.wav'],
                                [wls / '2']),
                       Workload('mp4-resize-720p',
                                ['-y', '-i', wls / '3' / 'in.mp4', '-vf', 'scale=1280:720',
                                 wls / '3' / 'out_720p.mp4'],
                                [wls / '3']),
                       Workload('jpegs-to-mp4',
                                ['-y', '-framerate', '30', '-i', wls / '4' / 'img%1d.jpg', '-c:v', 'libx264',
                                 wls / '4' / 'out_video.mp4'], [wls / '4']),
                       Workload('mp4-trim',
                                ['-y', '-i', wls / '5' / 'in.mp4', '-t', '0.06', '-c', 'copy',
                                 wls / '5' / 'out_trimmed.mp4'],
                                [wls / '5']),
                       Workload('mp4-flip-vertical',
                                ['-y', '-i', wls / '6' / 'in.mp4', '-vf', 'vflip', wls / '6' / 'out_flip.mp4'],
                                [wls / '6']),
                       Workload('mp4s-concat',
                                ['-y', '-f', 'concat', '-safe', '0', '-i', wls / '7' / 'list.txt', '-c', 'copy',
                                 wls / '7' / 'out_concat.mp4'], [wls / '7']),
                       Workload('mp4-stream-copy',
                                ['-y', '-i', wls / '8' / 'in.mp4', '-c', 'copy', wls / '8' / 'out_copy.mp4'],
                                [wls / '8']),
                       Workload('mp4-to-pngs',
                                ['-y', '-i', wls / '9' / 'in.mp4', '-vf', 'fps=1', wls / '9' / 'frame_%03d.png'],
                                [wls / '9']),
                       Workload('mp4-speedup-2x',
                                ['-y', '-i', wls / '10' / 'in.mp4', '-filter:v', 'setpts=0.5*PTS',
                                 wls / '10' / 'out_2x.mp4'],
                                [wls / '10']),
                       Workload('mp4-slowdown-2x',
                                ['-y', '-i', wls / '11' / 'in.mp4', '-filter:v', 'setpts=2.0*PTS',
                                 wls / '11' / 'out_slow.mp4'],
                                [wls / '11']),
                       Workload('mp4-mono-audio',
                                ['-y', '-i', wls / '12' / 'in.mp4', '-ac', '1', wls / '12' / 'out_mono.mp4'],
                                [wls / '12']),
                       Workload('mp4-remove-audio',
                                ['-y', '-i', wls / '13' / 'in.mp4', '-an', wls / '13' / 'out_noaudio.mp4'],
                                [wls / '13']),
                       Workload('mp4-to-flac',
                                ['-y', '-i', wls / '14' / 'in.mp4', '-vn', '-c:a', 'flac',
                                 wls / '14' / 'out_audio_only.flac'],
                                [wls / '14']),
                       Workload('mp4-gaussian-blur',
                                ['-y', '-i', wls / '15' / 'in.mp4', '-vf', 'gblur=sigma=10',
                                 wls / '15' / 'out_blur.mp4'],
                                [wls / '15'])]
                   )

gcc_loops = Benchmark('gcc-loops', PROJECT_ROOT / 'benchmarks/jetstream/gcc_loops/gcc-loops.wasm',
                      [Workload('default', [], [])])

wls = PROJECT_ROOT / 'benchmarks/sqlite/workloads'
sqlite = Benchmark('sqlite', PROJECT_ROOT / 'benchmarks/sqlite/sqlite.wasm',
                   [
                       Workload('create-insert-select',
                                [wls / '0' / 'db.sqlite', '<', wls / '0' / 'workload.sql'],
                                [wls / '0']),
                       Workload('bulk-insert-transaction',
                                [wls / '1' / 'db.sqlite', '<', wls / '1' / 'workload.sql'],
                                [wls / '1']),
                       Workload('select-where-index',
                                [wls / '2' / 'db.sqlite', '<', wls / '2' / 'workload.sql'],
                                [wls / '2']),
                       Workload('join-two-tables',
                                [wls / '3' / 'db.sqlite', '<', wls / '3' / 'workload.sql'],
                                [wls / '3']),
                       Workload('multi-table-join',
                                [wls / '4' / 'db.sqlite', '<', wls / '4' / 'workload.sql'],
                                [wls / '4']),
                       Workload('group-by-aggregate',
                                [wls / '5' / 'db.sqlite', '<', wls / '5' / 'workload.sql'],
                                [wls / '5']),
                       Workload('order-by-limit',
                                [wls / '6' / 'db.sqlite', '<', wls / '6' / 'workload.sql'],
                                [wls / '6']),
                       Workload('subquery-correlated',
                                [wls / '7' / 'db.sqlite', '<', wls / '7' / 'workload.sql'],
                                [wls / '7']),
                       Workload('with-recursive-cte',
                                [wls / '8' / 'db.sqlite', '<', wls / '8' / 'workload.sql'],
                                [wls / '8']),
                       Workload('full-text-search',
                                [wls / '9' / 'db.sqlite', '<', wls / '9' / 'workload.sql'],
                                [wls / '9']),
                       Workload('update-delete',
                                [wls / '10' / 'db.sqlite', '<', wls / '10' / 'workload.sql'],
                                [wls / '10']),
                       Workload('window-functions',
                                [wls / '11' / 'db.sqlite', '<', wls / '11' / 'workload.sql'],
                                [wls / '11']),
                       Workload('savepoint-rollback',
                                [wls / '12' / 'db.sqlite', '<', wls / '12' / 'workload.sql'],
                                [wls / '12']),
                       Workload('trigger-on-insert',
                                [wls / '13' / 'db.sqlite', '<', wls / '13' / 'workload.sql'],
                                [wls / '13']),
                       Workload('json-extract',
                                [wls / '14' / 'db.sqlite', '<', wls / '14' / 'workload.sql'],
                                [wls / '14']),
                       Workload('vacuum-analyze',
                                [wls / '15' / 'db.sqlite', '<', wls / '15' / 'workload.sql'],
                                [wls / '15']),
                   ]
                   )
