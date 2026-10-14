#include "src/kv_service/socket_utils.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/log/log.h"
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

absl::StatusOr<ReadResult> ReadFromClient(std::string& mutable_buffer,
                                          std::size_t offset,
                                          std::size_t payload_len,
                                          int client_fd) {
  mutable_buffer.resize(offset + payload_len);
  std::size_t pos = offset;
  ReadResult result;

  while (result.bytes_read < payload_len) {
    ssize_t n = recv(client_fd, mutable_buffer.data() + pos,
                     payload_len - result.bytes_read, 0);
    if (n == 0) {
      if (result.bytes_read == 0) {
        result.state = ReadState::kEOF;
        return result;
      }
      return absl::InternalError(
          absl::StrFormat("Dropped client fd %d mid stream."));
    }

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        mutable_buffer.resize(offset + result.bytes_read);
        result.state = ReadState::kEAGAIN;
        return result;
      }
      return absl::ErrnoToStatus(
          errno,
          absl::StrFormat("Dropped client fd %d mid payload read", client_fd));
    }

    // n > 0 - update counters and continue the loop
    result.bytes_read += static_cast<std::size_t>(n);
    pos += static_cast<std::size_t>(n);
  }

  return result;
}

absl::StatusOr<std::size_t> WriteToClient(const std::string& buffer,
                                          std::size_t offset,
                                          std::size_t n_bytes, int client_fd) {
  if (buffer.size() < n_bytes + offset) {
    return absl::InvalidArgumentError(
        "Buffer size is too small for requested write");
  }

  std::size_t total_write = 0;
  std::size_t pos = offset;
  while (total_write < n_bytes) {
    ssize_t n = send(client_fd, buffer.data() + pos, n_bytes - total_write,
                     MSG_NOSIGNAL);
    if (n > 0) {
      total_write += static_cast<std::size_t>(n);
      pos += static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return total_write;
      }
      return absl::ErrnoToStatus(
          errno, absl::StrFormat("Fatal send on client fd %d", client_fd));
    }

    // send should not return zero in this loop. If it does there is
    // some logic error.
    return absl::InternalError(
          "Send returned zero bytes. This should never happen");
  }

  return total_write;
}
}  // namespace kvstore