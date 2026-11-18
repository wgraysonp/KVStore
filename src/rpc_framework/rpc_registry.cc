#include "src/rpc_framework/rpc_registry.h"

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "src/rpc_framework/rpc_interface.h"

namespace rpc {
void RpcRegistry::RegisterRpc(
    uint32_t rpc_id, std::unique_ptr<IHandlerFactory> handler_factory) {
  handler_factories_[rpc_id] = std::move(handler_factory);
}

absl::Status RpcRegistry::DispatchRequest(
    RawMessage&& raw_request,
    absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete) const {
  auto it = handler_factories_.find(raw_request.rpc_id);
  if (it == handler_factories_.end()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("No registered RPC with id %d", raw_request.rpc_id));
  }
  std::unique_ptr<IHandler> rpc_handler =
      it->second->CreateHandler(std::move(raw_request), std::move(on_complete));

  ABSL_RETURN_IF_ERROR(rpc_handler->HandleRequest());

  return absl::OkStatus();
}
}  // namespace rpc