#include "VirtualMachine/VirtualMachine.h"
#include <uvwasi.h>

//// WASI ////
void VirtualMachine::args_get() {
  auto argv_offset = *(u32 *)(ActivationFrame.data() + 0);
  auto argv_buf_offset = *(u32 *)(ActivationFrame.data() + 4);
  std::vector<char *> argv(Wasi->argc);
  auto argv_buf = (char *)LinearMemory + argv_buf_offset;
  uvwasi_errno_t err = uvwasi_args_get(Wasi, argv.data(), argv_buf);
  if (err == UVWASI_ESUCCESS)
    for (auto i = 0; i < Wasi->argc; i++) {
      u32 offset = static_cast<u32>(argv_buf_offset + (argv[i] - argv[0]));
      uvwasi_serdes_write_uint32_t(
          LinearMemory, argv_offset + (i * UVWASI_SERDES_SIZE_uint32_t),
          offset);
    }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] args_get(argv_offset={}, argv_buf_offset={}): err={}\n",
             argv_offset, argv_buf_offset, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::args_sizes_get() {
  auto argc_offset = *(u32 *)(ActivationFrame.data() + 0);
  auto argv_buf_size_offset = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_size_t argc;
  uvwasi_size_t argv_buf_size;
  uvwasi_errno_t err = uvwasi_args_sizes_get(Wasi, &argc, &argv_buf_size);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_serdes_write_size_t(LinearMemory, argc_offset, argc);
    uvwasi_serdes_write_size_t(LinearMemory, argv_buf_size_offset,
                               argv_buf_size);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] args_sizes_get(argc_offset={}, argv_buf_size_offset={}): "
             "err={}\n",
             argc_offset, argv_buf_size_offset, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::clock_res_get() {
  auto clock_id = *(u32 *)(ActivationFrame.data() + 0);
  auto resolution_ptr = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_timestamp_t resolution;
  uvwasi_errno_t err = uvwasi_clock_res_get(Wasi, clock_id, &resolution);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_timestamp_t(LinearMemory, resolution_ptr, resolution);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] clock_res_get(clock_id={}, resolution_ptr={}): err={}\n",
             clock_id, resolution_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::clock_time_get() {
  auto clock_id = *(u32 *)(ActivationFrame.data() + 0);
  // VERY IMPORTANT to keep u64; caused a very bad bug!
  auto precision = *(u64 *)(ActivationFrame.data() + 4);
  auto time_ptr = *(u32 *)(ActivationFrame.data() + 12);
  uvwasi_timestamp_t time;
  uvwasi_errno_t err = uvwasi_clock_time_get(Wasi, clock_id, precision, &time);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_timestamp_t(LinearMemory, time_ptr, time);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] clock_time_get(clock_id={}, precision={}, time_ptr={}): "
             "err={}\n",
             clock_id, precision, time_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::environ_get() { // FIXME: check correctness
  auto environ_ptr = *(u32 *)(ActivationFrame.data() + 0);
  auto environ_buf_ptr = *(u32 *)(ActivationFrame.data() + 4);
  std::vector<char *> env(Wasi->argc);
  auto environ_buf = (char *)LinearMemory + environ_buf_ptr;
  uvwasi_errno_t err = uvwasi_environ_get(Wasi, env.data(), environ_buf);
  if (err == UVWASI_ESUCCESS)
    for (auto i = 0; i < Wasi->argc; i++) {
      u32 offset = static_cast<u32>(environ_buf_ptr + (env[i] - env[0]));
      uvwasi_serdes_write_uint32_t(
          LinearMemory, environ_ptr + (i * UVWASI_SERDES_SIZE_uint32_t),
          offset);
    }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] environ_get(environ_ptr={}, environ_buf_ptr={}): err={}\n",
             environ_ptr, environ_buf_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::environ_sizes_get() { // FIXME: check correctness
  auto environ_count_ptr = *(u32 *)(ActivationFrame.data() + 0);
  auto environ_buf_size_ptr = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_size_t environ_count;
  uvwasi_size_t environ_buf_size;
  uvwasi_errno_t err =
      uvwasi_environ_sizes_get(Wasi, &environ_count, &environ_buf_size);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_serdes_write_size_t(LinearMemory, environ_count_ptr, environ_count);
    uvwasi_serdes_write_size_t(LinearMemory, environ_buf_size_ptr,
                               environ_buf_size);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] environ_sizes_get(environ_count_ptr={}, "
             "environ_buf_size_ptr={}): "
             "err={}\n",
             environ_count_ptr, environ_buf_size_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_advise() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto offset = *(int64_t *)(ActivationFrame.data() + 4);
  auto len = *(int64_t *)(ActivationFrame.data() + 12);
  auto advice = *(u32 *)(ActivationFrame.data() + 20);
  auto err = uvwasi_fd_advise(Wasi, fd, offset, len, advice);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_advise(fd={}, offset={}, len={}, advice={}): err={}\n",
             fd, offset, len, advice, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_allocate() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto offset = *(int64_t *)(ActivationFrame.data() + 4);
  auto len = *(int64_t *)(ActivationFrame.data() + 12);
  auto err = uvwasi_fd_allocate(Wasi, fd, offset, len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_allocate(fd={}, offset={}, len={}): err={}\n", fd,
             offset, len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_close() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto err = uvwasi_fd_close(Wasi, fd);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold), "[wasi] fd_close(fd={}): err={}\n", fd, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_datasync() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto err = uvwasi_fd_datasync(Wasi, fd);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold), "[wasi] fd_datasync(fd={}): err={}\n", fd,
             err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_fdstat_get() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto buf = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_fdstat_t stats;
  uvwasi_errno_t err = uvwasi_fd_fdstat_get(Wasi, fd, &stats);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_fdstat_t(LinearMemory, buf, &stats);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_fdstat_get(fd={}, buf={}): err={}\n", fd, buf, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_fdstat_set_flags() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto flags = *(u32 *)(ActivationFrame.data() + 4);
  auto err = uvwasi_fd_fdstat_set_flags(Wasi, fd, flags);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_fdstat_set_flags(fd={}, flags={}): err={}\n", fd, flags,
             err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_fdstat_set_rights() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto fs_rights_base = *(u64 *)(ActivationFrame.data() + 4);
  auto fs_rights_inheriting = *(u64 *)(ActivationFrame.data() + 12);
  auto err = uvwasi_fd_fdstat_set_rights(Wasi, fd, fs_rights_base,
                                         fs_rights_inheriting);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_fdstat_set_rights(fd={}, fs_rights_base={}, "
             "fs_rights_inheriting={}): err={}\n",
             fd, fs_rights_base, fs_rights_inheriting, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_filestat_get() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto buf = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_filestat_t stats;
  uvwasi_errno_t err = uvwasi_fd_filestat_get(Wasi, fd, &stats);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_filestat_t(LinearMemory, buf, &stats);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_filestat_get(fd={}, buf={}): err={}\n", fd, buf, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_filestat_set_size() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto st_size = *(u64 *)(ActivationFrame.data() + 4);
  auto err = uvwasi_fd_filestat_set_size(Wasi, fd, st_size);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_filestat_set_size(fd={}, st_size={}): err={}\n", fd,
             st_size, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_filestat_set_times() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto st_atim = *(u64 *)(ActivationFrame.data() + 4);
  auto st_mtim = *(u64 *)(ActivationFrame.data() + 12);
  auto fstflags = *(u16 *)(ActivationFrame.data() + 20);
  auto err = uvwasi_fd_filestat_set_times(Wasi, fd, st_atim, st_mtim, fstflags);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_filestat_set_times(fd={}, st_atim={}, st_mtim={}, "
             "fstflags={}): err={}\n",
             fd, st_atim, st_mtim, fstflags, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_pread() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto iovs_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto iovs_len = *(u32 *)(ActivationFrame.data() + 8);
  auto offset = *(u64 *)(ActivationFrame.data() + 12);
  auto nread_ptr = *(u32 *)(ActivationFrame.data() + 20);

  std::vector<uvwasi_iovec_t> iovs(iovs_len);
  uvwasi_errno_t err = uvwasi_serdes_readv_iovec_t(
      LinearMemory, LinearMemorySize, iovs_ptr, iovs.data(), iovs_len);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_size_t nread;
    err = uvwasi_fd_pread(Wasi, fd, iovs.data(), iovs_len, offset, &nread);
    if (err == UVWASI_ESUCCESS)
      uvwasi_serdes_write_size_t(LinearMemory, nread_ptr, nread);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_pread(fd={}, iovs_ptr={}, iovs_len={}, offset={}, "
             "nread_ptr={}): err={}\n",
             fd, iovs_ptr, iovs_len, offset, nread_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_prestat_get() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto buf = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_prestat_t prestat;
  uvwasi_errno_t err = uvwasi_fd_prestat_get(Wasi, fd, &prestat);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_prestat_t(LinearMemory, buf, &prestat);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_prestat_get(fd={}, buf={}): err={}\n", fd, buf, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_prestat_dir_name() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto path_len = *(u32 *)(ActivationFrame.data() + 8);
  auto err =
      uvwasi_fd_prestat_dir_name(Wasi, fd, LinearMemory + path_ptr, path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_prestat_dir_name(fd={}, path_ptr={}, path_len={}): "
             "err={}\n",
             fd, path_ptr, path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_pwrite() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto iovs_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto iovs_len = *(u32 *)(ActivationFrame.data() + 8);
  auto offset = *(u64 *)(ActivationFrame.data() + 12);
  auto nwritten_ptr = *(u32 *)(ActivationFrame.data() + 20);

  std::vector<uvwasi_ciovec_t> iovs(iovs_len);
  uvwasi_errno_t err = uvwasi_serdes_readv_ciovec_t(
      LinearMemory, LinearMemorySize, iovs_ptr, iovs.data(), iovs_len);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_size_t nwritten;
    err = uvwasi_fd_pwrite(Wasi, fd, iovs.data(), iovs_len, offset, &nwritten);
    if (err == UVWASI_ESUCCESS)
      uvwasi_serdes_write_size_t(LinearMemory, nwritten_ptr, nwritten);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_pwrite(fd={}, iovs_ptr={}, iovs_len={}, offset={}, "
             "nwritten_ptr={}): err={}\n",
             fd, iovs_ptr, iovs_len, offset, nwritten_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_read() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto iovs_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto iovs_len = *(u32 *)(ActivationFrame.data() + 8);
  auto nread_ptr = *(u32 *)(ActivationFrame.data() + 12);

  std::vector<uvwasi_iovec_t> iovs(iovs_len);
  uvwasi_errno_t err = uvwasi_serdes_readv_iovec_t(
      LinearMemory, LinearMemorySize, iovs_ptr, iovs.data(), iovs_len);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_size_t nread;
    err = uvwasi_fd_read(Wasi, fd, iovs.data(), iovs_len, &nread);
    if (err == UVWASI_ESUCCESS)
      uvwasi_serdes_write_size_t(LinearMemory, nread_ptr, nread);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_read(fd={}, iovs_ptr={}, iovs_len={}, "
             "nread_ptr={}): err={}\n",
             fd, iovs_ptr, iovs_len, nread_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_readdir() { // FIXME: check correctness
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto buf = *(u32 *)(ActivationFrame.data() + 4);
  auto buf_len = *(u32 *)(ActivationFrame.data() + 8);
  auto cookie = *(u64 *)(ActivationFrame.data() + 12);
  auto buf_used_ptr = *(u32 *)(ActivationFrame.data() + 20);
  uvwasi_size_t buf_used;
  uvwasi_errno_t err = uvwasi_fd_readdir(Wasi, fd, LinearMemory + buf, buf_len,
                                         cookie, &buf_used);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_size_t(LinearMemory, buf_used_ptr, buf_used);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_readdir(fd={}, buf={}, buf_len={}, cookie={}, "
             "buf_used_ptr={}): err={}\n",
             fd, buf, buf_len, cookie, buf_used_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_renumber() {
  auto from = *(u32 *)(ActivationFrame.data() + 0);
  auto to = *(u32 *)(ActivationFrame.data() + 4);
  auto err = uvwasi_fd_renumber(Wasi, from, to);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_renumber(from={}, to={}): err={}\n", from, to, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_seek() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto offset = *(int64_t *)(ActivationFrame.data() + 4);
  auto whence = *(u32 *)(ActivationFrame.data() + 12);
  auto newoffset_ptr = *(u32 *)(ActivationFrame.data() + 16);
  uvwasi_filesize_t newoffset;
  uvwasi_errno_t err = uvwasi_fd_seek(Wasi, fd, offset, whence, &newoffset);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_filesize_t(LinearMemory, newoffset_ptr, newoffset);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_seek(fd={}, offset={}, whence={}, newoffset_ptr={}): "
             "err={}\n",
             fd, offset, whence, newoffset_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_sync() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto err = uvwasi_fd_sync(Wasi, fd);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold), "[wasi] fd_sync(fd={}): err={}\n", fd, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_tell() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto offset_ptr = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_filesize_t offset;
  uvwasi_errno_t err = uvwasi_fd_tell(Wasi, fd, &offset);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_filesize_t(LinearMemory, offset_ptr, offset);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_tell(fd={}, offset_ptr={}): err={}\n", fd, offset_ptr,
             err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::fd_write() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto iovs_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto iovs_len = *(u32 *)(ActivationFrame.data() + 8);
  auto nwritten_ptr = *(u32 *)(ActivationFrame.data() + 12);

  std::vector<uvwasi_ciovec_t> iovs(iovs_len);
  uvwasi_errno_t err = uvwasi_serdes_readv_ciovec_t(
      LinearMemory, LinearMemorySize, iovs_ptr, iovs.data(), iovs_len);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_size_t nwritten;
    err = uvwasi_fd_write(Wasi, fd, iovs.data(), iovs_len, &nwritten);
    if (err == UVWASI_ESUCCESS)
      uvwasi_serdes_write_size_t(LinearMemory, nwritten_ptr, nwritten);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] fd_write(fd={}, iovs_ptr={}, iovs_len={}, "
             "nwritten_ptr={}): err={}\n",
             fd, iovs_ptr, iovs_len, nwritten_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_create_directory() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto path_len = *(u32 *)(ActivationFrame.data() + 8);
  auto err =
      uvwasi_path_create_directory(Wasi, fd, LinearMemory + path_ptr, path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_create_directory(fd={}, path_ptr={}, path_len={}): "
             "err={}\n",
             fd, path_ptr, path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_filestat_get() { // FIXME: check correctness
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto flags = *(u32 *)(ActivationFrame.data() + 4);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 8);
  auto path_len = *(u32 *)(ActivationFrame.data() + 12);
  auto buf = *(u32 *)(ActivationFrame.data() + 16);
  uvwasi_filestat_t stats;
  uvwasi_errno_t err = uvwasi_path_filestat_get(
      Wasi, fd, static_cast<uvwasi_lookupflags_t>(flags),
      LinearMemory + path_ptr, path_len, &stats);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_filestat_t(LinearMemory, buf, &stats);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_filestat_get(fd={}, flags={}, path_ptr={}, "
             "path_len={}, buf={}): err={}\n",
             fd, flags, path_ptr, path_len, buf, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_filestat_set_times() { // FIXME: check correctness
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto flags = *(u32 *)(ActivationFrame.data() + 4);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 8);
  auto path_len = *(u32 *)(ActivationFrame.data() + 12);
  auto st_atim = *(u64 *)(ActivationFrame.data() + 16);
  auto st_mtim = *(u64 *)(ActivationFrame.data() + 24);
  auto fst_flags = *(u32 *)(ActivationFrame.data() + 32);
  uvwasi_filestat_t stats;
  uvwasi_errno_t err = uvwasi_path_filestat_set_times(
      Wasi, fd, static_cast<uvwasi_lookupflags_t>(flags),
      LinearMemory + path_ptr, path_len, st_atim, st_mtim,
      static_cast<uvwasi_fstflags_t>(fst_flags));
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_filestat_set_times(fd={}, flags={}, path_ptr={}, "
             "path_len={}, st_atim={}, st_mtim={}, fst_flags={}): err={}\n",
             fd, flags, path_ptr, path_len, st_atim, st_mtim, fst_flags, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_link() {
  auto old_fd = *(u32 *)(ActivationFrame.data() + 0);
  auto old_flags = *(u32 *)(ActivationFrame.data() + 4);
  auto old_path_ptr = *(u32 *)(ActivationFrame.data() + 8);
  auto old_path_len = *(u32 *)(ActivationFrame.data() + 12);
  auto new_fd = *(u32 *)(ActivationFrame.data() + 16);
  auto new_path_ptr = *(u32 *)(ActivationFrame.data() + 20);
  auto new_path_len = *(u32 *)(ActivationFrame.data() + 24);
  auto err = uvwasi_path_link(Wasi, old_fd,
                              static_cast<uvwasi_lookupflags_t>(old_flags),
                              LinearMemory + old_path_ptr, old_path_len, new_fd,
                              LinearMemory + new_path_ptr, new_path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_link(old_fd={}, old_flags={}, old_path_ptr={}, "
             "old_path_len={}, new_fd={}, new_path_ptr={}, new_path_len={}): "
             "err={}\n",
             old_fd, old_flags, old_path_ptr, old_path_len, new_fd,
             new_path_ptr, new_path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_open() {
  auto dirfd = *(u32 *)(ActivationFrame.data() + 0);
  auto dirflags = *(u32 *)(ActivationFrame.data() + 4);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 8);
  auto path_len = *(u32 *)(ActivationFrame.data() + 12);
  auto o_flags = *(u32 *)(ActivationFrame.data() + 16);
  auto fs_rights_base = *(u64 *)(ActivationFrame.data() + 20);
  auto fs_rights_inheriting = *(u64 *)(ActivationFrame.data() + 28);
  auto fs_flags = *(u32 *)(ActivationFrame.data() + 36);
  auto fd_ptr = *(u32 *)(ActivationFrame.data() + 40);
  uvwasi_fd_t fd;
  uvwasi_errno_t err = uvwasi_path_open(
      Wasi, dirfd, dirflags, LinearMemory + path_ptr, path_len,
      static_cast<uvwasi_oflags_t>(o_flags), fs_rights_base,
      fs_rights_inheriting, static_cast<uvwasi_fdflags_t>(fs_flags), &fd);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_size_t(LinearMemory, fd_ptr, fd);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_open(dirfd={}, dirflags={}, path_ptr={}, "
             "path_len={}, o_flags={}, fs_rights_base={}, "
             "fs_rights_inheriting={}, fs_flags={}, fd_ptr={}): err={}\n",
             dirfd, dirflags, path_ptr, path_len, o_flags, fs_rights_base,
             fs_rights_inheriting, fs_flags, fd_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_readlink() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto path_len = *(u32 *)(ActivationFrame.data() + 8);
  auto buf = *(u32 *)(ActivationFrame.data() + 12);
  auto buf_len = *(u32 *)(ActivationFrame.data() + 16);
  auto buf_used_ptr = *(u32 *)(ActivationFrame.data() + 20);
  uvwasi_size_t buf_used;
  uvwasi_errno_t err =
      uvwasi_path_readlink(Wasi, fd, LinearMemory + path_ptr, path_len,
                           LinearMemory + buf, buf_len, &buf_used);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_size_t(LinearMemory, buf_used_ptr, buf_used);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_readlink(fd={}, path_ptr={}, path_len={}, buf={}, "
             "buf_len={}, buf_used_ptr={}): err={}\n",
             fd, path_ptr, path_len, buf, buf_len, buf_used_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_remove_directory() {
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto path_len = *(u32 *)(ActivationFrame.data() + 8);
  auto err =
      uvwasi_path_remove_directory(Wasi, fd, LinearMemory + path_ptr, path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_remove_directory(fd={}, path_ptr={}, path_len={}): "
             "err={}\n",
             fd, path_ptr, path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_rename() {
  auto old_fd = *(u32 *)(ActivationFrame.data() + 0);
  auto old_path_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto old_path_len = *(u32 *)(ActivationFrame.data() + 8);
  auto new_fd = *(u32 *)(ActivationFrame.data() + 12);
  auto new_path_ptr = *(u32 *)(ActivationFrame.data() + 16);
  auto new_path_len = *(u32 *)(ActivationFrame.data() + 20);
  auto err = uvwasi_path_rename(Wasi, old_fd, LinearMemory + old_path_ptr,
                                old_path_len, new_fd,
                                LinearMemory + new_path_ptr, new_path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_rename(old_fd={}, old_path_ptr={}, old_path_len={}, "
             "new_fd={}, new_path_ptr={}, new_path_len={}): err={}\n",
             old_fd, old_path_ptr, old_path_len, new_fd, new_path_ptr,
             new_path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_symlink() {
  auto old_path_ptr = *(u32 *)(ActivationFrame.data() + 0);
  auto old_path_len = *(u32 *)(ActivationFrame.data() + 4);
  auto fd = *(u32 *)(ActivationFrame.data() + 8);
  auto new_path_ptr = *(u32 *)(ActivationFrame.data() + 12);
  auto new_path_len = *(u32 *)(ActivationFrame.data() + 16);
  auto err =
      uvwasi_path_symlink(Wasi, LinearMemory + old_path_ptr, old_path_len, fd,
                          LinearMemory + new_path_ptr, new_path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_symlink(old_path_ptr={}, old_path_len={}, fd={}, "
             "new_path_ptr={}, new_path_len={}): err={}\n",
             old_path_ptr, old_path_len, fd, new_path_ptr, new_path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::path_unlink_file() { // FIXME: check correctness
  auto fd = *(u32 *)(ActivationFrame.data() + 0);
  auto path_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto path_len = *(u32 *)(ActivationFrame.data() + 8);
  auto err =
      uvwasi_path_unlink_file(Wasi, fd, LinearMemory + path_ptr, path_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] path_unlink_file(fd={}, path_ptr={}, path_len={}): "
             "err={}\n",
             fd, path_ptr, path_len, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::poll_oneoff() { // FIXME: check correctness
  auto in_ptr = *(u32 *)(ActivationFrame.data() + 0);
  auto out_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto nsubscriptions = *(u32 *)(ActivationFrame.data() + 8);
  auto nevents_ptr = *(u32 *)(ActivationFrame.data() + 12);
  std::vector<uvwasi_subscription_t> in(nsubscriptions);
  std::vector<uvwasi_event_t> out(nsubscriptions);
  for (uint32_t i = 0; i < nsubscriptions; ++i) {
    uvwasi_serdes_read_subscription_t(LinearMemory, in_ptr, &in[i]);
    in_ptr += UVWASI_SERDES_SIZE_subscription_t;
  }

  uvwasi_size_t nevents;
  uvwasi_errno_t err =
      uvwasi_poll_oneoff(Wasi, in.data(), out.data(), nsubscriptions, &nevents);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_serdes_write_size_t(LinearMemory, nevents_ptr, nevents);

    for (uint32_t i = 0; i < nsubscriptions; ++i) {
      uvwasi_serdes_write_event_t(LinearMemory, out_ptr, &out[i]);
      out_ptr += UVWASI_SERDES_SIZE_event_t;
    }
  }

  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] poll_oneoff(in_ptr={}, out_ptr={}, nsubscriptions={}, "
             "nevents_ptr={}): err={}\n",
             in_ptr, out_ptr, nsubscriptions, nevents_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::proc_exit() {
  auto code = *(u32 *)(ActivationFrame.data() + 0);
  uvwasi_proc_exit(Wasi, code);
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold), "[wasi] proc_exit(code={})\n", code);
  std::cout << std::flush;
#endif
}
void VirtualMachine::proc_raise() {
  auto sig = *(u32 *)(ActivationFrame.data() + 0);
  uvwasi_errno_t err = uvwasi_proc_raise(Wasi, sig);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold), "[wasi] proc_raise(sig={}): err={}\n", sig,
             err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::random_get() { // FIXME: check correctness
  auto buf = *(u32 *)(ActivationFrame.data() + 0);
  auto buf_len = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_errno_t err = uvwasi_random_get(Wasi, LinearMemory + buf, buf_len);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] random_get(buf={}, buf_len={}): err={}\n", buf, buf_len,
             err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::sched_yield() {
  uvwasi_errno_t err = uvwasi_sched_yield(Wasi);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold), "[wasi] sched_yield(): err={}\n", err);
  std::cout << std::flush;
#endif
}

void VirtualMachine::sock_accept() {
  auto sock = *(u32 *)(ActivationFrame.data() + 0);
  auto flags = *(u32 *)(ActivationFrame.data() + 4);
  auto fd_ptr = *(u32 *)(ActivationFrame.data() + 8);

  uvwasi_fd_t fd;
  uvwasi_errno_t err = uvwasi_sock_accept(Wasi, sock, flags, &fd);
  if (err == UVWASI_ESUCCESS)
    uvwasi_serdes_write_size_t(LinearMemory, fd_ptr, fd);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] sock_accept(sock={}, flags={}, fd_ptr={}): err={}\n", sock,
             flags, fd_ptr, err);
#endif
}

void VirtualMachine::sock_recv() {
  auto sock = *(u32 *)(ActivationFrame.data() + 0);
  auto ri_data_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto ri_data_len = *(u32 *)(ActivationFrame.data() + 8);
  auto ri_flags = *(u32 *)(ActivationFrame.data() + 12);
  auto ro_datalen_ptr = *(u32 *)(ActivationFrame.data() + 16);
  auto ro_flags_ptr = *(u32 *)(ActivationFrame.data() + 20);

  std::vector<uvwasi_iovec_t> ri_data(ri_data_len);
  uvwasi_errno_t err = uvwasi_serdes_readv_iovec_t(
      LinearMemory, LinearMemorySize, ri_data_ptr, ri_data.data(), ri_data_len);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_size_t ro_datalen;
    uvwasi_roflags_t ro_flags;
    err = uvwasi_sock_recv(Wasi, sock, ri_data.data(), ri_data_len, ri_flags,
                           &ro_datalen, &ro_flags);
    if (err == UVWASI_ESUCCESS) {
      uvwasi_serdes_write_size_t(LinearMemory, ro_datalen_ptr, ro_datalen);
      uvwasi_serdes_write_roflags_t(LinearMemory, ro_flags_ptr, ro_flags);
    }
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(
      fg(fmt::color::gold),
      "[wasi] sock_recv(sock={}, ri_data_ptr={}, ri_data_len={}, ri_flags={}, "
      "ro_datalen_ptr={}, ro_flags_ptr={}): err={}\n",
      sock, ri_data_ptr, ri_data_len, ri_flags, ro_datalen_ptr, ro_flags_ptr,
      err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::sock_send() {
  auto sock = *(u32 *)(ActivationFrame.data() + 0);
  auto si_data_ptr = *(u32 *)(ActivationFrame.data() + 4);
  auto si_data_len = *(u32 *)(ActivationFrame.data() + 8);
  auto si_flags = *(u32 *)(ActivationFrame.data() + 12);
  auto so_datalen_ptr = *(u32 *)(ActivationFrame.data() + 16);

  std::vector<uvwasi_ciovec_t> si_data(si_data_len);
  uvwasi_errno_t err = uvwasi_serdes_readv_ciovec_t(
      LinearMemory, LinearMemorySize, si_data_ptr, si_data.data(), si_data_len);
  if (err == UVWASI_ESUCCESS) {
    uvwasi_size_t so_datalen;
    err = uvwasi_sock_send(Wasi, sock, si_data.data(), si_data_len, si_flags,
                           &so_datalen);
    if (err == UVWASI_ESUCCESS)
      uvwasi_serdes_write_size_t(LinearMemory, so_datalen_ptr, so_datalen);
  }
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(
      fg(fmt::color::gold),
      "[wasi] sock_send(sock={}, si_data_ptr={}, si_data_len={}, si_flags={}, "
      "so_datalen_ptr={}): err={}\n",
      sock, si_data_ptr, si_data_len, si_flags, so_datalen_ptr, err);
  std::cout << std::flush;
#endif
}
void VirtualMachine::sock_shutdown() {
  auto sock = *(u32 *)(ActivationFrame.data() + 0);
  auto how = *(u32 *)(ActivationFrame.data() + 4);
  uvwasi_errno_t err = uvwasi_sock_shutdown(Wasi, sock, how);
  *(u32 *)(ActivationFrame.data() + 0) = err;
#ifdef PRINT_WASI
  fmt::print(fg(fmt::color::gold),
             "[wasi] sock_shutdown(sock={}, how={}): "
             "err={}\n",
             sock, how, err);
  std::cout << std::flush;
#endif
}
