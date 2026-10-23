#ifndef ENDPOINT_CLIENT_CONNECTION_H_
#define ENDPOINT_CLIENT_CONNECTION_H_

#include <unistd.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/endpoint/socket_utils.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_registry.h"

namespace endpoint {

enum class ConnectionStatus {
  CONNECTED_READ = 1,
  CONNECTED_DISPATCH = 2,
  CONNECTED_WRITE = 3,
  DISCONNECTED_ERROR = 4,
  DISCONNECTED_SUCCESS = 5,
};

class IClientConnection {
 public:
  virtual ~IClientConnection() = default;
  virtual absl::Status MakeNonBlocking() = 0;
  virtual absl::StatusOr<ConnectionStatus> HandleEvent() = 0;
};

class ClientConnection : public IClientConnection {
 public:
  explicit ClientConnection(
      int client_fd, std::shared_ptr<const rpc::IRpcRegistry> rpc_registry)
      : client_fd_(client_fd), rpc_registry_(std::move(rpc_registry)) {}
  ClientConnection& operator=(const ClientConnection&) = delete;
  ClientConnection(const ClientConnection&) = delete;
  ClientConnection& operator=(ClientConnection&&) noexcept = default;
  ClientConnection(ClientConnection&&) noexcept = default;
  ~ClientConnection() override {
    if (client_fd_ != -1) close(client_fd_);
  }

  absl::Status MakeNonBlocking() override;
  absl::StatusOr<ConnectionStatus> HandleEvent() override;

  ConnectionStatus GetConnectionStatus() const { return connection_status_; }

 private:
  int client_fd_ = -1;
  ConnectionStatus connection_status_ = ConnectionStatus::CONNECTED_READ;
  std::size_t size_header_bytes_read_ = 0;
  std::size_t rpc_id_header_bytes_read_ = 0;
  std::size_t payload_bytes_read_ = 0;
  std::size_t response_header_bytes_written_ = 0;
  std::size_t response_bytes_written_ = 0;
  std::string size_header_buffer_;
  std::string rpc_id_buffer_;
  std::string payload_buffer_;
  std::string response_header_buffer_;
  std::string response_buffer_;
  std::shared_ptr<const rpc::IRpcRegistry> rpc_registry_;

  ConnectionStatus ProcessReadStage();
  ConnectionStatus ProcessDispatchStage();
  ConnectionStatus ProcessWriteStage();
  absl::StatusOr<ReadState> PerformClientRead();
  absl::StatusOr<WriteState> PerformClientWrite();

  // creates a raw message struct from the read_buffers to send to rpc routing,
  // clears the intrenal read buffers, and sets bytes_read trackers to zero.
  absl::StatusOr<rpc::RawMessage> MoveClientReadDataToRawMessage();

  void HandleDisconnectOnError(const absl::Status& status = absl::OkStatus());

  void HandleError(const absl::Status& status);
  // reset the connection buffers and counters
  void Reset();
};

class IClientConnectionFactory {
 public:
  virtual ~IClientConnectionFactory() = default;
  virtual std::unique_ptr<IClientConnection> CreateClientConnection(
      int client_fd, std::shared_ptr<const rpc::IRpcRegistry> rpc_registry) = 0;
};

class ClientConnectionFactory : public IClientConnectionFactory {
 public:
  std::unique_ptr<IClientConnection> CreateClientConnection(
      int client_fd,
      std::shared_ptr<const rpc::IRpcRegistry> rpc_registry) override {
    return std::make_unique<ClientConnection>(client_fd,
                                               std::move(rpc_registry));
  }
};

}  // namespace endpoint

#endif  // ENDPOINT_CLIENT_CONNECTION_H_