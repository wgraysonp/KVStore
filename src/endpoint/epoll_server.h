#ifndef ENDPOINT_EPOLL_SERVER_H_
#define ENDPOINT_EPOLL_SERVER_H_

#include <sys/epoll.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest_prod.h"
#include "src/rpc_framework/rpc_registry.h"

namespace endpoint {

class EpollServer {
 public:
  explicit EpollServer(int port,
                       std::unique_ptr<rpc::IRpcRegistry> rpc_registry,
                       bool local_only = true)
      : rpc_registry_(std::move(rpc_registry)),
        port_(port),
        local_only_(local_only) {};

  ~EpollServer();
  EpollServer(const EpollServer&) = delete;
  EpollServer& operator=(const EpollServer&) = delete;

  bool IsRunning() const { return is_running_; }

  // start the server
  absl::Status Start();

  // blocking event loop
  absl::Status RunLoop();

  void Stop();

 private:
  FRIEND_TEST(EpollServerTest, SetNonBlockingFailsWithInvalidFD);

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
  absl::Status HandleCurrentConnection(int client_fd);

  absl::Status HandleClientWrite(int client_fd, const std::string& response);

  std::unique_ptr<rpc::IRpcRegistry> rpc_registry_;
  int port_;
  int listen_fd_ = -1;
  int epoll_fd_ = -1;
  const int max_events_ = 64;
  std::atomic<bool> is_running_ = false;
  bool local_only_ = true;
};
}  // namespace endpoint

#endif  // ENDPOINT_EPOLL_SERVER_H_