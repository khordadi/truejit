from pathlib import Path

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = "FFmpeg"
binary = directory / 'ffmpeg.wasm'
wls = directory / 'workloads'
workloads = [Workload('mov-to-mp4', ['-y', '-i', wls / '0' / 'in.mov', wls / '0' / 'out.mp4'], [wls / '0']),
             Workload('mp4-to-mkv', ['-y', '-i', wls / '1' / 'in.mp4', '-c:v', 'libx264', wls / '1' / 'out.mkv'],
                      [wls / '1']),
             Workload('mp4-to-wav', ['-y', '-i', wls / '2' / 'in.mp4', '-c:a', 'pcm_s16le', wls / '2' / 'out.wav'],
                      [wls / '2']),
             Workload('mp4-resize-720p',
                      ['-y', '-i', wls / '3' / 'in.mp4', '-vf', 'scale=1280:720', wls / '3' / 'out_720p.mp4'],
                      [wls / '3']),
             Workload('jpegs-to-mp4', ['-y', '-framerate', '30', '-i', wls / '4' / 'img%1d.jpg', '-c:v', 'libx264',
                                       wls / '4' / 'out_video.mp4'], [wls / '4']),
             Workload('mp4-trim',
                      ['-y', '-i', wls / '5' / 'in.mp4', '-t', '0.06', '-c', 'copy', wls / '5' / 'out_trimmed.mp4'],
                      [wls / '5']),
             Workload('mp4-flip-vertical',
                      ['-y', '-i', wls / '6' / 'in.mp4', '-vf', 'vflip', wls / '6' / 'out_flip.mp4'], [wls / '6']),
             Workload('mp4s-concat', ['-y', '-benchmarks_list', 'concat', '-safe', '0', '-i', wls / '7' / 'list.txt', '-c', 'copy',
                                      wls / '7' / 'out_concat.mp4'], [wls / '7']),
             Workload('mp4-stream-copy', ['-y', '-i', wls / '8' / 'in.mp4', '-c', 'copy', wls / '8' / 'out_copy.mp4'],
                      [wls / '8']),
             Workload('mp4-to-pngs', ['-y', '-i', wls / '9' / 'in.mp4', '-vf', 'fps=1', wls / '9' / 'frame_%03d.png'],
                      [wls / '9']),
             Workload('mp4-speedup-2x',
                      ['-y', '-i', wls / '10' / 'in.mp4', '-filter:v', 'setpts=0.5*PTS', wls / '10' / 'out_2x.mp4'],
                      [wls / '10']),
             Workload('mp4-slowdown-2x',
                      ['-y', '-i', wls / '11' / 'in.mp4', '-filter:v', 'setpts=2.0*PTS', wls / '11' / 'out_slow.mp4'],
                      [wls / '11']),
             Workload('mp4-mono-audio', ['-y', '-i', wls / '12' / 'in.mp4', '-ac', '1', wls / '12' / 'out_mono.mp4'],
                      [wls / '12']),
             Workload('mp4-remove-audio', ['-y', '-i', wls / '13' / 'in.mp4', '-an', wls / '13' / 'out_noaudio.mp4'],
                      [wls / '13']),
             Workload('mp4-to-flac',
                      ['-y', '-i', wls / '14' / 'in.mp4', '-vn', '-c:a', 'flac', wls / '14' / 'out_audio_only.flac'],
                      [wls / '14']),
             Workload('mp4-gaussian-blur',
                      ['-y', '-i', wls / '15' / 'in.mp4', '-vf', 'gblur=sigma=10', wls / '15' / 'out_blur.mp4'],
                      [wls / '15'])]
ffmpeg = Benchmark(name, binary, workloads)
