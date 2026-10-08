#ifndef SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_FACTORY_H_
#define SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_FACTORY_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

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
    ON_CALL(*this, CreateHandler()).WillByDefault([]() {
      auto mock_handler = std::make_unique<::testing::NiceMock<MockHandler>>();
      return mock_handler;
    });
  };
  ~MockHandlerFactory() override = default;

  MOCK_METHOD((std::unique_ptr<IHandler>), CreateHandler, (), (override));
};
}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_TESTS_MOCK_HANDLER_FACTORY_H_