#ifndef ENDPOINT_CONNECTION_MANAGER_H_
#define ENDPOINT_CONNECTION_MANAGER_H_

#include <sys/epoll.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "src/endpoint/client_connection.h"
#include "src/rpc_framework/rpc_registry.h"

namespace endpoint {

class ConnectionManager {
 public:
  explicit ConnectionManager(int port,
                             std::shared_ptr<rpc::IRpcRegistry> rpc_registry)
      : rpc_registry_(std::move(rpc_registry)), port_(port) {};
  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;
  ConnectionManager(ConnectionManager&&) = delete;
  ConnectionManager& operator=(ConnectionManager&&) = delete;
  ~ConnectionManager();

  // bind to port and start listening
  absl::Status BindAndListen();

  // forever event loop
  absl::Status RunLoop();

  void Stop();

 private:
   // Process all active events either accepting a new client connection
  // or passing event to existing client connection
  void ProcessActiveEpollEvents(const std::vector<epoll_event>& events);

  // accept a new client connection
  absl::Status AcceptNewConnection();

  absl::Status HandleExistingClientEvent(const epoll_event& ev);

  absl::flat_hash_map<int, ClientConnection> active_connections_;
  std::shared_ptr<const rpc::IRpcRegistry> rpc_registry_;
  std::atomic<bool> is_running_;
  int port_;
  int listen_fd_ = -1;
  int epoll_fd_ = -1;
  const int max_events_ = 64;
};
}  // namespace endpoint

#endif  // ENDPOINT_CONNECTION_MANAGER_H_