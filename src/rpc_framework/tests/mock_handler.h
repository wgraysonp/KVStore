#ifndef SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_H_
#define SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"

namespace rpc {
class MockHandler : public IHandler {
 public:
  MockHandler(bool handle_request_returns_error = false) {
    if (handle_request_returns_error) {
      ON_CALL(*this, HandleRequest(::testing::_))
          .WillByDefault(::testing::Return(
              absl::InternalError("A terrible error has occured.")));
    } else {
      ON_CALL(*this, HandleRequest(::testing::_))
          .WillByDefault([](RawMessage raw_request) {
            return absl::StrCat("Hello ", raw_request.request_payload);
          });
    }
  }
  ~MockHandler() override = default;

  MOCK_METHOD((absl::StatusOr<std::string>), HandleRequest,
              (RawMessage raw_request), (override));

  MOCK_METHOD((absl::Status), RunSync, (), (override));
};
}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_H_