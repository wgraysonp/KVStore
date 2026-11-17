#ifndef SRC_RPC_FRAMEWORK_TESTS_MOCK_RPC_REGISTRY_H_
#define SRC_RPC_FRAMEWORK_TESTS_MOCK_RPC_REGISTRY_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"
#include "src/rpc_framework/rpc_registry.h"

namespace rpc {
class MockRpcRegistry : public IRpcRegistry {
 public:
  MockRpcRegistry() {
    ON_CALL(*this, DispatchRequest(::testing::_, ::testing::_))
        .WillByDefault([](RawMessage raw_request,
                          absl::AnyInvocable<void(std::string)> on_complete) {
          std::string response =
              absl::StrCat("Hello ", raw_request.request_payload);
          on_complete(std::move(response));
          return absl::OkStatus();
        });
  };
  ~MockRpcRegistry() override = default;

  MOCK_METHOD((void), RegisterRpc,
              (uint32_t rpc_id, std::unique_ptr<IHandlerFactory>), (override));
  MOCK_METHOD((absl::Status), DispatchRequest,
              (RawMessage && raw_request,
               absl::AnyInvocable<void(std::string)> on_complete),
              (const override));
};
}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_TESTS_MOCK_RPC_REGISTRY_H_