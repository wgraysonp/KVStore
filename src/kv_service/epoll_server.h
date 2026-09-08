#ifndef KVSTORE_SRC_SERVICE_EPOLL_SERVER_H_
#define KVSTORE_SRC_SERVICE_EPOLL_SERVER_H_

#include <sys/epoll.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"

namespace kvstore {

class EpollServer {
 public:
  // TODO - change this later to functions that dont just accept and return
  // strings. Maybe abseil has something I should be using for futures?
  using MessageHandler = std::function<std::string(const std::string&)>;

  explicit EpollServer(int port, MessageHandler handler, bool local_only = true)
      : message_handler_(std::move(handler)),
        port_(port),
        local_only_(local_only) {};

  ~EpollServer();
  EpollServer(const EpollServer&) = delete;
  EpollServer& operator=(const EpollServer&) = delete;

  // start the server
  absl::Status Start();

  // blocking event loop
  absl::Status RunLoop();

  void Stop();

 private:
  // Set a socket file descriptor to non-blocking
  absl::Status SetNonBlocking(int fd);

  // Process all active events either accepting a new client connection
  // or reading from an existing client
  void ProcessActiveEpollEvents(const std::vector<epoll_event>& events,
                                int num_fds);

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
  const int max_events_ = 64;
  bool is_running_ = false;
  bool local_only_ = true;
};
}  // namespace kvstore

#endif  // KVSTORE_SRC_SERVICE_EPOLL_SERVER_H_