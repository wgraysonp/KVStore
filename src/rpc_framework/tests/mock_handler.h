#ifndef SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_H_
#define SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"

namespace rpc {
class MockHandler : public IHandler {
 public:
  MockHandler(RawMessage raw_message,
              absl::AnyInvocable<void(std::string)> on_complete,
              bool handle_request_returns_error = false)
      : raw_message_(std::move(raw_message)),
        on_complete_(std::move(on_complete)) {
    if (handle_request_returns_error) {
      ON_CALL(*this, HandleRequest())
          .WillByDefault(::testing::Return(
              absl::InternalError("A terrible error has occured.")));
    } else {
      ON_CALL(*this, HandleRequest()).WillByDefault([this]() {
        return absl::OkStatus();
      });
    }
  }
  ~MockHandler() override = default;

  MOCK_METHOD((absl::Status), HandleRequest, (), (override));

 private:
  RawMessage raw_message_;
  absl::AnyInvocable<void(std::string)> on_complete_;
};
}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_H_