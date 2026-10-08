#ifndef SRC_RPC_FRAMEWORK_RPC_REGISTRY_H_
#define SRC_RPC_FRAMEWORK_RPC_REGISTRY_H_

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "src/rpc_framework/rpc_interface.h"

namespace rpc {

class RpcRegistry {
 public:
  RpcRegistry() = default;
  RpcRegistry(const RpcRegistry&) = delete;
  RpcRegistry& operator=(const RpcRegistry&) = delete;
  RpcRegistry(RpcRegistry&&) = delete;
  RpcRegistry operator=(RpcRegistry&&) = delete;
  ~RpcRegistry() = default;

  void RegisterRpc(uint32_t rpc_id,
                   std::unique_ptr<IHandlerFactory> handler_factory);

  absl::StatusOr<std::string> DispatchRequest(RawMessage raw_request);

  const absl::flat_hash_map<uint32_t, std::unique_ptr<IHandlerFactory>>&
  GetRegisteredRpcMap() const {
    return handler_factories_;
  }

 private:
  absl::flat_hash_map<uint32_t, std::unique_ptr<IHandlerFactory>>
      handler_factories_;
};

}  // namespace rpc

#endif  // SRC_RPC_FRAMEORK_RPC_REGISTRY_H_