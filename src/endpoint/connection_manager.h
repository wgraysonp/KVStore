#ifndef ENDPOINT_CONNECTION_MANAGER_H_
#define ENDPOINT_CONNECTION_MANAGER_H_

#include <sys/epoll.h>

#include <atomic>
#include <cstddef>
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
  explicit ConnectionManager(
      int port, std::shared_ptr<rpc::IRpcRegistry> rpc_registry,
      std::unique_ptr<IClientConnectionFactory> connection_factory)
      : rpc_registry_(std::move(rpc_registry)),
        connection_factory_(std::move(connection_factory)),
        port_(port) {
          events_.reserve(max_events_);
        };

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;
  ConnectionManager(ConnectionManager&&) = delete;
  ConnectionManager& operator=(ConnectionManager&&) = delete;
  ~ConnectionManager();

  // bind to port and start listening
  absl::Status BindAndListen();

  // forever event loop
  absl::Status LoopForever();

  absl::Status LoopOnce(int time_out);

  void Stop();

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
  GetActiveConnections() const {
    return active_connections_;
  }

 private:
  // Process all active events either accepting a new client connection
  // or passing event to existing client connection
  void ProcessActiveEpollEvents(const std::vector<epoll_event>& events);

  // accept a new client connection
  absl::Status AcceptNewConnection();

  absl::Status HandleExistingClientEvent(const epoll_event& ev);

  std::vector<epoll_event> events_;
  absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>
      active_connections_;
  std::shared_ptr<const rpc::IRpcRegistry> rpc_registry_;
  std::unique_ptr<IClientConnectionFactory> connection_factory_;
  std::atomic<bool> is_running_;
  int port_;
  int listen_fd_ = -1;
  int epoll_fd_ = -1;
  const int max_events_ = 64;
};
}  // namespace endpoint

#endif  // ENDPOINT_CONNECTION_MANAGER_H_