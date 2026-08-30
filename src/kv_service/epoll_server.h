#ifndef KVSTORE_SRC_SERVICE_EPOLL_SERVER_H_
#define KVSTORE_SRC_SERVICE_EPOLL_SERVER_H_

#include <cstdint>

#include "absl/status/status.h"

namespace kvstore {

class EpollServer {
 public:
  // TODO - change this later to functions that dont just accept and return
  // strings. Maybe abseil has something I should be using for futures?
  using MessageHandler = std::function<std::string(const std::string&)>;

  explicit EpollServer(int port, MessageHandler handler)
      : port_(port), message_handler_(std::move(handler)) {};

  // start the server
  absl::Status Start();

  // blocking event loop
  void RunLoop();

  void Stop();

 private:
  // Set a socket file descriptor to non-blocking
  absl::Status SetNonBlocking(int fd);

  // Convert 4 bytes from big-endian network byte order to host byte order
  uint32_t ReadUint32(const char* buffer);

  // accept a new client connection
  absl::Status AcceptNewConnection();

  // Handle data coming from an existing client connection
  absl::Status HandleClientRead(int client_fd);

  MessageHandler message_handler_;
  int port_;
  int listen_fd_ = -1;
  int epoll_fd_ = -1;
  const int max_events = 64;
  bool is_running = false;
};
}  // namespace kvstore

#endif  // KVSTORE_SRC_SERVICE_EPOLL_SERVER_H_