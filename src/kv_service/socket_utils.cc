#include "src/kv_service/socket_utils.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/status/statusor.h"

namespace kvstore {

absl::Status SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return absl::ErrnoToStatus(
        errno, absl::StrFormat("Failed to read flags for descriptor %d", fd));
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return absl::ErrnoToStatus(
        errno, absl::StrFormat(
                   "Failed to write O_NONBLOCK flag for descriptor %d", fd));
  }
  return absl::OkStatus();
}

uint32_t ReadUint32(const std::string& buffer) {
  uint32_t val;
  std::memcpy(&val, buffer.data(), 4);
  return ntohl(val);
}

absl::StatusOr<std::size_t> ReadFromClient(std::string& mutable_buffer,
                                       std::size_t offset,
                                       std::size_t payload_len, int client_fd) {
  mutable_buffer.resize(offset + payload_len);
  std::size_t total_read = 0;
  std::size_t pos = offset;
  while (total_read < payload_len) {
    ssize_t n = recv(client_fd, mutable_buffer.data() + pos,
                     payload_len - total_read, 0);
    if (n > 0) {
      total_read += static_cast<std::size_t>(n);
      pos += static_cast<std::size_t>(n);
      continue;
    }

    if (n == 0) {
      return absl::InternalError(
          absl::StrFormat("Dropped client fd %d mid stream."));
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      mutable_buffer.resize(offset + total_read);
      return total_read;
    }

    // n < 0 and errno is not EAGAIN or EWOULDBLOCK - fatal system error
    return absl::ErrnoToStatus(
        errno,
        absl::StrFormat("Dropped client fd %d mid payload read", client_fd));
  }

  return total_read;
}
}  // namespace kvstore