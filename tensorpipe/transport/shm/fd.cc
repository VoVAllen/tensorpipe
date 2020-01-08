#include <tensorpipe/transport/shm/fd.h>

#include <unistd.h>

#include <tensorpipe/common/defs.h>
#include <tensorpipe/transport/error_macros.h>

namespace tensorpipe {
namespace transport {
namespace shm {

Fd::~Fd() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

ssize_t Fd::read(void* buf, size_t count) {
  ssize_t rv = -1;
  for (;;) {
    rv = ::read(fd_, buf, count);
    if (rv == -1 && errno == EINTR) {
      continue;
    }
    break;
  }
  return rv;
}

// Proxy to write(2) with EINTR retry.
ssize_t Fd::write(const void* buf, size_t count) {
  ssize_t rv = -1;
  for (;;) {
    rv = ::write(fd_, buf, count);
    if (rv == -1 && errno == EINTR) {
      continue;
    }
    break;
  }
  return rv;
}

// Call read and throw if it doesn't complete.
Error Fd::readFull(void* buf, size_t count) {
  auto rv = read(buf, count);
  if (rv == -1) {
    return TP_CREATE_ERROR(SystemError, "read", errno);
  }
  if (rv != count) {
    return TP_CREATE_ERROR(ShortReadError, count, rv);
  }
  return Error::kSuccess;
}

// Call write and throw if it doesn't complete.
Error Fd::writeFull(const void* buf, size_t count) {
  auto rv = write(buf, count);
  if (rv == -1) {
    return TP_CREATE_ERROR(SystemError, "write", errno);
  }
  if (rv != count) {
    return TP_CREATE_ERROR(ShortWriteError, count, rv);
  }
  return Error::kSuccess;
}

} // namespace shm
} // namespace transport
} // namespace tensorpipe
