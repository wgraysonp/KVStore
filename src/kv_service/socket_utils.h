#ifndef SRC_KVSERVICE_SOCKET_UTILS_H_
#define SRC_KVSERVICE_SOCKET_UTILS_H_

#include <fcntl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/status/statusor.h"

namespace kvstore {

enum class ReadState {
  kSUCCESS = 1, // successfully read all requested bytes
  kEAGAIN = 2, // partial read stopped due to errno EAGAIN or EWOULDBLOCK. 
  kEOF = 3, // zero bytes read and got EOF packet
};

struct ReadResult {
  ReadState state = ReadState::kSUCCESS;
  std::size_t bytes_read = 0;
};

// set a file descriptor to non blocking mode
absl::Status SetNonBlocking(int fd);

// Convert 4 bytes from big-endian network byte order to host byte order
uint32_t ReadUint32(const std::string& buffer);

// read at most payload_len bytes from client_fd into mutable buffer starting at
// position mutable_buffer.data() + offset and returns a ReadResult struct
absl::StatusOr<ReadResult> ReadFromClient(std::string& mutable_buffer,
                                           std::size_t offset,
                                           std::size_t payload_len,
                                           int client_fd);

// write at moust n_bytes to client_fd from buffer starting at position
// buffer.data() + offset returns the total number of bytes written or an error
// code. If errno is set to EAGAIN or EWOULDBLOCK this is not considered a
// failure. the number of bytes written may be less than requested
absl::StatusOr<std::size_t> WriteToClient(const std::string& buffer,
                                          std::size_t offset,
                                          std::size_t n_bytes, int client_fd);

}  // namespace kvstore

#endif  // SRC_KVSERVICE_SOCKET_UTILS_H_