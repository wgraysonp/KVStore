#ifndef SRC_RPC_FRAMEWORK_TESTS_MOCK_RPC_REGISTRY_H_
#define SRC_RPC_FRAMEWORK_TESTS_MOCK_RPC_REGISTRY_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"
#include "src/rpc_framework/rpc_registry.h"

namespace rpc {
class MockRpcRegistry : public IRpcRegistry {
 public:
  MockRpcRegistry() {
    ON_CALL(*this, DispatchRequest(::testing::_))
        .WillByDefault([](RawMessage raw_request) {
          return absl::StrCat("Hello ", raw_request.request_payload);
        });
  };
  ~MockRpcRegistry() override = default;

  MOCK_METHOD((void), RegisterRpc,
              (uint32_t rpc_id, std::unique_ptr<IHandlerFactory>), (override));
  MOCK_METHOD((absl::StatusOr<std::string>), DispatchRequest,
              (RawMessage raw_request), (override));
};
}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_TESTS_MOCK_RPC_REGISTRY_H_