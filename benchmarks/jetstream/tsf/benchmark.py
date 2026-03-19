from multiprocessing import cpu_count
import subprocess
from multiprocessing.pool import ThreadPool
from pathlib import Path

from Benchmarking import Workload, Benchmark

directory = Path(__file__).parent

name = 'tsf'
binary = directory / 'tsf.wasm'
workloads = [Workload()]
tsf = Benchmark(name, binary, workloads)


def install():
    src_dir.mkdir(exist_ok=True)
    tsf_srcs = ['tsf_asprintf.c', 'tsf_buffer.c', 'tsf_error.c', 'tsf_reflect.c', 'tsf_st.c', 'tsf_type.c',
                'tsf_io.c', 'tsf_native.c', 'tsf_generator.c', 'tsf_st_typetable.c', 'tsf_parser.c',
                'tsf_buf_writer.c', 'tsf_buf_reader.c', 'tsf_primitive.c', 'tsf_type_table.c',
                'tsf_copier.c',
                'tsf_destructor.c', 'tsf_gpc_code_gen.c', 'gpc_code_gen_util.c', 'gpc_threaded.c',
                'gpc_intable.c', 'gpc_instruction.c', 'gpc_program.c', 'gpc_proto.c', 'gpc_stack_height.c',
                'tsf_serial_in_man.c', 'tsf_serial_out_man.c', 'tsf_type_in_map.c', 'tsf_type_out_map.c',
                'tsf_stream_file_input.c', 'tsf_stream_file_output.c', 'tsf_sort.c', 'tsf_version.c',
                'tsf_named_type.c', 'tsf_io_utils.c', 'tsf_zip_attr.c', 'tsf_zip_reader.c',
                'tsf_zip_writer.c',
                'tsf_zip_abstract.c', 'tsf_limits.c', 'tsf_ra_type_man.c', 'tsf_adaptive_reader.c',
                'tsf_sha1.c', 'tsf_sha1_writer.c', 'tsf_fsdb.c', 'tsf_fsdb_protocol.c',
                'tsf_define_helpers.c',
                'tsf_ir.c', 'tsf_ir_different.c', 'tsf_ir_speed.c']
    tsf_files = tsf_srcs + ['config.h', 'gpc_worklist.h', 'tsf_config_stub.h', 'tsf.h', 'tsf_internal.h',
                            'tsf_region.h', 'tsf_types.h', 'gpc.h', 'tsf_atomics.h',
                            'tsf_define_helpers.h', 'tsf_indent.h', 'tsf_inttypes.h',
                            'tsf_serial_protocol.h', 'tsf_util.h',
                            'gpc_int_common.h', 'tsf_build_defines.h', 'tsf_format.h',
                            'tsf_internal_config.h',
                            'tsf_ir_different.h', 'tsf_sha1.h', 'tsf_zip_abstract.h', 'gpc_internal.h',
                            'tsf_config.h', 'tsf_fsdb_protocol.h', 'tsf_internal_config_stub.h',
                            'tsf_ir.h',
                            'tsf_st.h', 'gpc_instruction_dispatch.gen',
                            'gpc_instruction_stack_effects.gen',
                            'gpc_instruction_to_string.gen', 'gpc_instruction_size.gen',
                            'gpc_instruction_static_size.gen', 'gpc_interpreter.gen']

    download_one = lambda file: subprocess.run(
        ['wget', '-N', f'https://browserbench.org/JetStream/wasm/TSF/{file}'], cwd=src_dir, check=True)

    results = ThreadPool(cpu_count()).imap_unordered(download_one, tsf_files)
    for _ in results:
        pass

    with open(directory / 'tsf.patch', 'r') as patch:
        subprocess.run(['patch', '-p1', '-N'], input=patch.read().encode(), cwd=src_dir, check=True)

    return tsf_srcs


def build(tsf_srcs):
    # [very important] use -O0 to avoid the bug in truejit
    compiler = '/opt/wasi-sdk/bin/clang'
    flags = ['-O0',
             '-z', 'stack-size=1048576',
             '-I.'
             '-Wl,--initial-memory=52428800',
             '-Wl,--export=main',
             '-Wl,--export=__heap_base',
             '-Wl,--export=__data_end',
             '-Wl,--allow-undefined',
             '-DTSF_BUILD_SYSTEM=1',
             '-Wno-implicit-function-declaration']
    sources = tsf_srcs
    subprocess.run([compiler, *flags, *sources, '-o', binary], cwd=src_dir, check=True)


def uninstall():
    if src_dir.exists():
        subprocess.run(['rm', '-rf', src_dir], check=True)


if __name__ == '__main__':
    src_dir = directory / 'src'

    tsf_srcs = install()
    build(tsf_srcs)
    uninstall()
