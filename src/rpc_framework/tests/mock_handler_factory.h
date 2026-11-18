#ifndef SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_FACTORY_H_
#define SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_FACTORY_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"
#include "src/rpc_framework/tests/mock_handler.h"

namespace rpc {
class MockHandlerFactory : public IHandlerFactory {
 public:
  MockHandlerFactory() {
    ON_CALL(*this, CreateHandler(::testing::_, ::testing::_))
        .WillByDefault([](RawMessage&& raw_message,
                          absl::AnyInvocable<void(absl::StatusOr<std::string>)>
                              on_complete) {
          auto mock_handler =
              std::make_unique<::testing::NiceMock<MockHandler>>(
                  std::move(raw_message), std::move(on_complete));
          return mock_handler;
        });
  };
  ~MockHandlerFactory() override = default;

  MOCK_METHOD(
      (std::unique_ptr<IHandler>), CreateHandler,
      (RawMessage && raw_message,
       absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete),
      (override));
};
}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_FACTORY_H_