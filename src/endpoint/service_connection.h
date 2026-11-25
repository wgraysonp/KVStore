#ifndef ENDPOINT_SERVICE_CONNECTION_H_
#define ENDPOINT_SERVICE_CONNECTION_H_

#include <cstdint>
#include <queue>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/common/consts.h"
#include "src/common/utils/socket_utils.h"

namespace endpoint {

enum class ServiceIOStage {
  SIZE_HEADER = 1,
  RPC_ID = 2,
  REQUEST_ID = 3,
  PAYLOAD = 4,
};

struct ServiceIOPackage {
  std::string network_order_size_header;
  std::string network_order_rpc_id;
  std::string network_order_request_id;
  std::string payload_bytes;
  ServiceIOStage stage = ServiceIOStage::SIZE_HEADER;
  std::size_t size_header_bytes_processed = 0;
  std::size_t rpc_id_bytes_processed = 0;
  std::size_t request_id_bytes_processed = 0;
  std::size_t payload_bytes_processed = 0;
};

class IServiceConnection {
 public:
  virtual ~IServiceConnection() = default;
  virtual absl::Status SendQueuedPackages() = 0;
  virtual absl::Status Receive() = 0;
  virtual absl::Status QueueOutboundRequest(
      uint32_t rpc_id, uint32_t request_id, std::string request_bytes,
      absl::AnyInvocable<void(std::string)> request_callback) = 0;
};

class ServiceConnection : public IServiceConnection {
 public:
  ServiceConnection(int service_fd)
      : service_fd_(service_fd) {
    read_buffer_.reserve(common::READ_BUFFER_SIZE);
  }

  ~ServiceConnection() override {
    if (service_fd_ != -1) close(service_fd_);
  }

  absl::Status SendQueuedPackages() override;
  absl::Status Receive() override;
  absl::Status QueueOutboundRequest(
      uint32_t rpc_id, uint32_t request_id, std::string request_bytes,
      absl::AnyInvocable<void(std::string)> request_callback) override;

 private:
  absl::StatusOr<common::WriteState> WriteSinglePackage(
      ServiceIOPackage& mutable_outbound_package);

  std::queue<ServiceIOPackage> outbound_package_queue_;
  absl::flat_hash_map<uint32_t, absl::AnyInvocable<void(std::string)>>
      request_id_to_callback_map_;
  std::string read_buffer_;
  int service_fd_ = -1;
};
}  // namespace endpoint

#endif ENDPOINT_SERVICE_CONNECTION_H_