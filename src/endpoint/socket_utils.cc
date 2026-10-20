#include "src/endpoint/socket_utils.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"

namespace endpoint {

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

absl::StatusOr<uint32_t> ReadUint32(const std::string& buffer) {
  if (buffer.size() != 4) {
    return absl::InvalidArgumentError("Buffer must be exactly four bytes.");
  }
  uint32_t val;
  std::memcpy(&val, buffer.data(), 4);
  return ntohl(val);
}

std::string ConvertUint32ToNetworkBytes(uint32_t num) {
  std::string bytes;
  bytes.resize(sizeof(uint32_t));
  uint32_t network_order_num = htonl(num);
  std::memcpy(bytes.data(), &network_order_num, sizeof(uint32_t));
  return bytes;
}

absl::StatusOr<ReadResult> ReadFromClient(std::string& mutable_buffer,
                                          const std::size_t offset,
                                          const std::size_t payload_len,
                                          const int client_fd) {
  mutable_buffer.resize(offset + payload_len);
  std::size_t pos = offset;
  ReadResult result;

  while (result.bytes_read < payload_len) {
    const ssize_t n = recv(client_fd, mutable_buffer.data() + pos,
                           payload_len - result.bytes_read, 0);
    if (n == 0) {
      if (result.bytes_read == 0) {
        mutable_buffer.resize(offset + result.bytes_read);
        result.state = ReadState::kEOF;
        return result;
      }
      mutable_buffer.resize(offset);
      return absl::CancelledError(absl::StrFormat(
          "Client fd %d closed connection cleanly mid read.", client_fd));
    }

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        mutable_buffer.resize(offset + result.bytes_read);
        result.state = ReadState::kEAGAIN;
        return result;
      }
      mutable_buffer.resize(offset);
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

absl::StatusOr<WriteResult> WriteToClient(const std::string& buffer,
                                          const std::size_t offset,
                                          const std::size_t n_bytes,
                                          const int client_fd) {
  if (buffer.size() < n_bytes + offset) {
    return absl::InvalidArgumentError(
        "Buffer size is too small for requested write");
  }

  WriteResult result;

  std::size_t pos = offset;
  while (result.bytes_written < n_bytes) {
    const ssize_t n = send(client_fd, buffer.data() + pos,
                           n_bytes - result.bytes_written, MSG_NOSIGNAL);
    if (n > 0) {
      result.bytes_written += static_cast<std::size_t>(n);
      pos += static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        result.state = WriteState::kEAGAIN;
        return result;
      }
      return absl::ErrnoToStatus(
          errno, absl::StrFormat("Fatal send on client fd %d", client_fd));
    }

    // send should not return zero in this loop. If it does there is
    // some logic error.
    return absl::InternalError(
        "Send returned zero bytes. This should never happen");
  }

  return result;
}

absl::Status WriteErrorMessageToClient(const std::string& error_message,
                                       int client_fd) {
  std::string error_header = ConvertUint32ToNetworkBytes(error_message.size());

  ABSL_ASSIGN_OR_RETURN(
      WriteResult unused_header_write_result,
      WriteToClient(error_header, 0, error_header.size(), client_fd));

  ABSL_ASSIGN_OR_RETURN(
      WriteResult unused_write_result,
      WriteToClient(error_message, 0, error_message.size(), client_fd));

  return absl::OkStatus();
}
}  // namespace endpoint