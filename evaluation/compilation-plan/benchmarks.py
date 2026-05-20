from pathlib import Path

from utils import Workload, Benchmark

PROJECT_ROOT = Path('/users/khordadi/truejit')
B = PROJECT_ROOT / 'benchmarks'

# ── FFmpeg ────────────────────────────────────────────────────────────────────
wls = B / 'ffmpeg/workloads'
ffmpeg = Benchmark('FFmpeg', B / 'ffmpeg/ffmpeg.wasm', [
    Workload('mov-to-mp4',        ['-y', '-i', wls/'0'/'in.mov',  wls/'0'/'out.mp4'],                                                          [wls/'0']),
    Workload('mp4-to-mkv',        ['-y', '-i', wls/'1'/'in.mp4',  '-c:v', 'libx264', wls/'1'/'out.mkv'],                                       [wls/'1']),
    Workload('mp4-to-wav',        ['-y', '-i', wls/'2'/'in.mp4',  '-c:a', 'pcm_s16le', wls/'2'/'out.wav'],                                     [wls/'2']),
    Workload('mp4-resize-720p',   ['-y', '-i', wls/'3'/'in.mp4',  '-vf', 'scale=1280:720', wls/'3'/'out_720p.mp4'],                            [wls/'3']),
    Workload('jpegs-to-mp4',      ['-y', '-framerate', '30', '-i', wls/'4'/'img%1d.jpg', '-c:v', 'libx264', wls/'4'/'out_video.mp4'],          [wls/'4']),
    Workload('mp4-trim',          ['-y', '-i', wls/'5'/'in.mp4',  '-t', '0.06', '-c', 'copy', wls/'5'/'out_trimmed.mp4'],                      [wls/'5']),
    Workload('mp4-flip-vertical', ['-y', '-i', wls/'6'/'in.mp4',  '-vf', 'vflip', wls/'6'/'out_flip.mp4'],                                     [wls/'6']),
    Workload('mp4s-concat',       ['-y', '-f', 'concat', '-safe', '0', '-i', wls/'7'/'list.txt', '-c', 'copy', wls/'7'/'out_concat.mp4'],      [wls/'7']),
    Workload('mp4-stream-copy',   ['-y', '-i', wls/'8'/'in.mp4',  '-c', 'copy', wls/'8'/'out_copy.mp4'],                                       [wls/'8']),
    Workload('mp4-to-pngs',       ['-y', '-i', wls/'9'/'in.mp4',  '-vf', 'fps=1', wls/'9'/'frame_%03d.png'],                                   [wls/'9']),
    Workload('mp4-speedup-2x',    ['-y', '-i', wls/'10'/'in.mp4', '-filter:v', 'setpts=0.5*PTS', wls/'10'/'out_2x.mp4'],                       [wls/'10']),
    Workload('mp4-slowdown-2x',   ['-y', '-i', wls/'11'/'in.mp4', '-filter:v', 'setpts=2.0*PTS', wls/'11'/'out_slow.mp4'],                     [wls/'11']),
    Workload('mp4-mono-audio',    ['-y', '-i', wls/'12'/'in.mp4', '-ac', '1', wls/'12'/'out_mono.mp4'],                                        [wls/'12']),
    Workload('mp4-remove-audio',  ['-y', '-i', wls/'13'/'in.mp4', '-an', wls/'13'/'out_noaudio.mp4'],                                          [wls/'13']),
    Workload('mp4-to-flac',       ['-y', '-i', wls/'14'/'in.mp4', '-vn', '-c:a', 'flac', wls/'14'/'out_audio_only.flac'],                      [wls/'14']),
    Workload('mp4-gaussian-blur', ['-y', '-i', wls/'15'/'in.mp4', '-vf', 'gblur=sigma=10', wls/'15'/'out_blur.mp4'],                           [wls/'15']),
], suite='FFmpeg')

# ── SQLite ────────────────────────────────────────────────────────────────────
wls = B / 'sqlite/workloads'
sqlite = Benchmark('SQLite', B / 'sqlite/sqlite.wasm', [
    Workload('create-insert-select',    [wls/'0'/'db.sqlite',  '<', wls/'0'/'workload.sql'],  [wls/'0']),
    Workload('bulk-insert-transaction', [wls/'1'/'db.sqlite',  '<', wls/'1'/'workload.sql'],  [wls/'1']),
    Workload('select-where-index',      [wls/'2'/'db.sqlite',  '<', wls/'2'/'workload.sql'],  [wls/'2']),
    Workload('join-two-tables',         [wls/'3'/'db.sqlite',  '<', wls/'3'/'workload.sql'],  [wls/'3']),
    Workload('multi-table-join',        [wls/'4'/'db.sqlite',  '<', wls/'4'/'workload.sql'],  [wls/'4']),
    Workload('group-by-aggregate',      [wls/'5'/'db.sqlite',  '<', wls/'5'/'workload.sql'],  [wls/'5']),
    Workload('order-by-limit',          [wls/'6'/'db.sqlite',  '<', wls/'6'/'workload.sql'],  [wls/'6']),
    Workload('subquery-correlated',     [wls/'7'/'db.sqlite',  '<', wls/'7'/'workload.sql'],  [wls/'7']),
    Workload('with-recursive-cte',      [wls/'8'/'db.sqlite',  '<', wls/'8'/'workload.sql'],  [wls/'8']),
    Workload('full-text-search',        [wls/'9'/'db.sqlite',  '<', wls/'9'/'workload.sql'],  [wls/'9']),
    Workload('update-delete',           [wls/'10'/'db.sqlite', '<', wls/'10'/'workload.sql'], [wls/'10']),
    Workload('window-functions',        [wls/'11'/'db.sqlite', '<', wls/'11'/'workload.sql'], [wls/'11']),
    Workload('savepoint-rollback',      [wls/'12'/'db.sqlite', '<', wls/'12'/'workload.sql'], [wls/'12']),
    Workload('trigger-on-insert',       [wls/'13'/'db.sqlite', '<', wls/'13'/'workload.sql'], [wls/'13']),
    Workload('json-extract',            [wls/'14'/'db.sqlite', '<', wls/'14'/'workload.sql'], [wls/'14']),
    Workload('vacuum-analyze',          [wls/'15'/'db.sqlite', '<', wls/'15'/'workload.sql'], [wls/'15']),
], suite='SQLite')

# ── CoreMark ──────────────────────────────────────────────────────────────────
coremark = Benchmark('CoreMark', B / 'coremark/coremark.wasm', [
    Workload('default', [], []),
], suite='CoreMark')

# ── JetStream ─────────────────────────────────────────────────────────────────
float_mm  = Benchmark('float-mm',  B / 'jetstream/float_mm/float-mm.wasm',    [Workload('default', [], [])], suite='JetStream')
gcc_loops = Benchmark('gcc-loops', B / 'jetstream/gcc_loops/gcc-loops.wasm',  [Workload('default', [], [])], suite='JetStream')
hashset   = Benchmark('hashset',   B / 'jetstream/hashset/hashset.wasm',       [Workload('default', [], [])], suite='JetStream')
quicksort = Benchmark('quicksort', B / 'jetstream/quicksort/quicksort.wasm',   [Workload('default', [], [])], suite='JetStream')
tsf       = Benchmark('tsf',       B / 'jetstream/tsf/tsf.wasm',               [Workload('default', [], [])], suite='JetStream')

jetstream = [float_mm, gcc_loops, hashset, quicksort, tsf]

# ── MiBench ───────────────────────────────────────────────────────────────────
_mb = B / 'mibench'

basicmath    = Benchmark('basicmath',    _mb / 'basicmath/basicmath.wasm',       [Workload('default', [], [])],                                                                                                                             suite='MiBench')
bitcount     = Benchmark('bitcount',     _mb / 'bitcount/bitcount.wasm',         [Workload('default', ['1125000'], [])],                                                                                                                    suite='MiBench')
stringsearch = Benchmark('stringsearch', _mb / 'stringsearch/stringsearch.wasm', [Workload('default', [], [])],                                                                                                                             suite='MiBench')
cjpeg        = Benchmark('cjpeg',        _mb / 'cjpeg/cjpeg.wasm',              [Workload('default', ['-dct', 'int', '-progressive', '-opt', '-outfile', 'output_large_encode.jpeg', 'input_large.ppm'], [_mb / 'cjpeg'])],              suite='MiBench')
djpeg        = Benchmark('djpeg',        _mb / 'djpeg/djpeg.wasm',              [Workload('default', ['-dct', 'int', '-ppm', '-outfile', 'output_large_decode.ppm', 'input_large.jpg'], [_mb / 'djpeg'])],                               suite='MiBench')
rawcaudio    = Benchmark('rawcaudio',    _mb / 'rawcaudio/rawcaudio.wasm',       [Workload('default', ['<', _mb / 'rawcaudio/large.pcm', '>', _mb / 'rawcaudio/output_large.adpcm'], [_mb / 'rawcaudio'])],                               suite='MiBench')
rawdaudio    = Benchmark('rawdaudio',    _mb / 'rawdaudio/rawdaudio.wasm',       [Workload('default', ['<', _mb / 'rawdaudio/large.adpcm', '>', _mb / 'rawdaudio/output_large.dpcm'], [_mb / 'rawdaudio'])],                              suite='MiBench')
blowfish_encode = Benchmark('blowfish-encode', _mb / 'blowfish/blowfish.wasm',  [Workload('default', ['e', 'input_large.asc', 'output_large.enc', '1234567890abcdeffedcba0987654321'], [_mb / 'blowfish/encode'])],                       suite='MiBench')
blowfish_decode = Benchmark('blowfish-decode', _mb / 'blowfish/blowfish.wasm',  [Workload('default', ['d', 'input_large.enc', 'output_large.asc', '1234567890abcdeffedcba0987654321'], [_mb / 'blowfish/decode'])],                       suite='MiBench')
rijndael_encode = Benchmark('rijndael-encode', _mb / 'rijndael/rijndael.wasm',  [Workload('default', [_mb / 'rijndael/encode/input_large.asc', _mb / 'rijndael/encode/output_large.enc', 'e', '1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321'], [_mb / 'rijndael/encode'])], suite='MiBench')
rijndael_decode = Benchmark('rijndael-decode', _mb / 'rijndael/rijndael.wasm',  [Workload('default', [_mb / 'rijndael/decode/input_large.enc', _mb / 'rijndael/decode/output_large.dec', 'd', '1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321'], [_mb / 'rijndael/decode'])], suite='MiBench')
crc32        = Benchmark('crc32',        _mb / 'crc32/crc32.wasm',              [Workload('default', ['large.pcm'], [_mb / 'crc32'])],                                                                                                     suite='MiBench')
sha          = Benchmark('sha',          _mb / 'sha/sha.wasm',                  [Workload('default', ['input_large.asc'], [_mb / 'sha'])],                                                                                                 suite='MiBench')

mibench = [basicmath, bitcount, stringsearch, cjpeg, djpeg, rawcaudio, rawdaudio,
           blowfish_encode, blowfish_decode, rijndael_encode, rijndael_decode, crc32, sha]

# ── NPB ───────────────────────────────────────────────────────────────────────
# Change NPB_CLASS to select a different dataset size (s, w, a, b, c, d).
NPB_CLASS = 'a'

_npb = B / 'npb'
# LU class 'a' crashes the VM with SIGSEGV; use class 'w' for it instead.
_npb_entries = [('bt', 'BT', NPB_CLASS), ('cg', 'CG', NPB_CLASS), ('ep', 'EP', NPB_CLASS),
                ('ft', 'FT', NPB_CLASS), ('is', 'IS', NPB_CLASS), ('lu', 'LU', 'w'),
                ('mg', 'MG', NPB_CLASS), ('sp', 'SP', NPB_CLASS)]
npb = [
    Benchmark(display, _npb / short / f'{cls}.wasm', [Workload('default', [], [])], suite='NPB')
    for short, display, cls in _npb_entries
]

# ── PolyBench ─────────────────────────────────────────────────────────────────
# Change POLYBENCH_DATASET to select a different input size
# (mini, small, medium, standard, large, extra-large).
POLYBENCH_DATASET = 'large'

_pb = B / 'polybench'
polybench = [
    Benchmark(name, _pb / name / f'{POLYBENCH_DATASET}.wasm', [Workload('default', [], [])], suite='PolyBench')
    for name in [
        'correlation', 'covariance',
        '2mm', '3mm', 'atax', 'bicg', 'doitgen', 'mvt',
        'gemm', 'gemver', 'gesummv', 'symm', 'syr2k', 'syrk', 'trmm',
        'cholesky', 'durbin', 'gramschmidt', 'lu', 'ludcmp', 'trisolv',
        'deriche', 'floyd-warshall', 'nussinov',
        'adi', 'fdtd-2d', 'heat-3d', 'jacobi-1d', 'jacobi-2d', 'seidel-2d',
    ]
]

# ── SPEC ──────────────────────────────────────────────────────────────────────
_sp = B / 'spec'

lbm       = Benchmark('lbm',       _sp / 'lbm/lbm.wasm',               [Workload('default', ['64', _sp / 'lbm/reference.dat', '0', '1', _sp / 'lbm/100_100_130_ldc.of'], [_sp / 'lbm'])],              suite='SPEC')
mcf       = Benchmark('mcf',       _sp / 'mcf/mcf.wasm',               [Workload('default', [_sp / 'mcf/inp.in'], [_sp / 'mcf'])],                                                                      suite='SPEC')
nab       = Benchmark('nab',       _sp / 'nab/nab.wasm',               [Workload('default', ['aminos', '391519156', '1000'], [_sp / 'nab'])],                                                            suite='SPEC')
namd      = Benchmark('namd',      _sp / 'namd/namd.wasm',             [Workload('default', ['--input', _sp / 'namd/apoa1.input', '--iterations', '2'], [_sp / 'namd'])],                               suite='SPEC')
x264      = Benchmark('x264',      _sp / 'x264/x264.wasm',             [Workload('default', ['--dumpyuv', '50', '--frames', '142', '-o', _sp / 'x264/in.264', _sp / 'x264/in.yuv', '1280x720'], [_sp / 'x264'])], suite='SPEC')
deepsjeng = Benchmark('deepsjeng', _sp / 'deepsjeng/deepsjeng.wasm',   [Workload('default', [_sp / 'deepsjeng/train.txt'], [_sp / 'deepsjeng'])],                                                       suite='SPEC')

spec = [lbm, mcf, nab, namd, x264, deepsjeng]

# ── WABench ───────────────────────────────────────────────────────────────────
_wa = B / 'wabench'

bzip2         = Benchmark('bzip2',         _wa / 'bzip2/bzip2.wasm',                 [Workload('default', ['-k', '-f', '-z', _wa / 'bzip2/bzip2.wasm'], [_wa / 'bzip2'])],                                   suite='WABench')
espeak        = Benchmark('espeak',        _wa / 'espeak/espeak.wasm',               [Workload('default', ['-f', _wa / 'espeak/input.txt', '-s', '120', '-w', _wa / 'espeak/output_file.wav'], [_wa / 'espeak'])], suite='WABench')
facedetection = Benchmark('facedetection', _wa / 'facedetection/facedetection.wasm', [Workload('default', [_wa / 'facedetection/input.png'], [_wa / 'facedetection'])],                                      suite='WABench')
gnuchess      = Benchmark('gnuchess',      _wa / 'gnuchess/gnuchess.wasm',           [Workload('default', ['<', _wa / 'gnuchess/input'], [])],                                                               suite='WABench')
mnist         = Benchmark('mnist',         _wa / 'mnist/mnist.wasm',                 [Workload('default', [], [_wa / 'mnist'])],                                                                             suite='WABench')
snappy        = Benchmark('snappy',        _wa / 'snappy/snappy.wasm',               [Workload('default', [], [])],                                                                                          suite='WABench')
whitedb       = Benchmark('whitedb',       _wa / 'whitedb/whitedb.wasm',             [Workload('default', [], [])],                                                                                          suite='WABench')

wabench = [bzip2, espeak, facedetection, gnuchess, mnist, snappy, whitedb]

# ── All benchmarks ────────────────────────────────────────────────────────────
all_benchmarks = [ffmpeg, sqlite, coremark, *jetstream, *mibench, *npb, *polybench, *spec, *wabench]
