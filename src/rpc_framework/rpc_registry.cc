#include "src/rpc_framework/rpc_registry.h"

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "src/rpc_framework/rpc_interface.h"

namespace rpc {
void RpcRegistry::RegisterRpc(
    uint32_t rpc_id, std::unique_ptr<IHandlerFactory> handler_factory) {
  handler_factories_[rpc_id] = std::move(handler_factory);
}

absl::StatusOr<std::string> RpcRegistry::DispatchRequest(
    RawMessage raw_request) {
  auto it = handler_factories_.find(raw_request.rpc_id);
  if (it == handler_factories_.end()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("No registered RPC with id %d", raw_request.rpc_id));
  }
  std::unique_ptr<IHandler> rpc_handler = it->second->CreateHandler();

  ABSL_ASSIGN_OR_RETURN(std::string response_bytes,
                        rpc_handler->HandleRequest(std::move(raw_request)));

  return response_bytes;
}
}  // namespace rpc