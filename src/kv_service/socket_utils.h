#ifndef SRC_KVSERVICE_SOCKET_UTILS_H_
#define SRC_KVSERVICE_SOCKET_UTILS_H_

#include <fcntl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/status/statusor.h"

namespace kvstore {

// set a file descriptor to non blocking mode
absl::Status SetNonBlocking(int fd);

// Convert 4 bytes from big-endian network byte order to host byte order
uint32_t ReadUint32(const std::string& buffer);

// read at most payload_len bytes from client_fd into mutable buffer starting at
// position mutable_buffer.data() + offset and returns the number of bytes read
// or an error code. If errno is set to EAGAIN or EWOULDBLOCK this is not
// considered a failure. total bytes will be returned and the number of bytes
// read may be less than requested
absl::StatusOr<std::size_t> ReadFromClient(std::string& mutable_buffer,
                                       std::size_t offset,
                                       std::size_t payload_len, int client_fd);

}  // namespace kvstore

#endif  // SRC_KVSERVICE_SOCKET_UTILS_H_