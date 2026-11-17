#include "src/rpc_framework/rpc_registry.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/tests/mock_handler.h"
#include "src/rpc_framework/tests/mock_handler_factory.h"

using ::absl_testing::StatusIs;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;

namespace rpc {

using RegistryMap =
    absl::flat_hash_map<uint32_t, std::unique_ptr<IHandlerFactory>>;

class RpcRegistryTest : public ::testing::Test {
 protected:
  std::unique_ptr<RpcRegistry> registry_;
  NiceMock<MockHandlerFactory>* mock_factory_;

  void SetUp() override { registry_ = std::make_unique<RpcRegistry>(); }
};

TEST_F(RpcRegistryTest, RpcRegistryCorrectlyRegistersRpc) {
  std::unique_ptr<MockHandlerFactory> movable_mock_factory =
      std::make_unique<NiceMock<MockHandlerFactory>>();
  registry_->RegisterRpc(1, std::move(movable_mock_factory));

  const RegistryMap& registry_map = registry_->GetRegisteredRpcMap();
  EXPECT_EQ(registry_map.size(), (size_t)1);
  EXPECT_TRUE(registry_map.contains(1));
}

TEST_F(RpcRegistryTest, RpcRegistryDispatchReturnsErrorForNonExistentRpc) {
  RawMessage msg{
      .rpc_id = 1,
      .payload_size = 10,
      .request_payload = "hello",
  };

  absl::AnyInvocable<void(std::string)> on_complete =
      [](std::string response_bytes) { return; };

  absl::Status response_status =
      registry_->DispatchRequest(std::move(msg), std::move(on_complete));
  EXPECT_THAT(response_status,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("No registered RPC with id 1")));
}

TEST_F(RpcRegistryTest, RpcRegistryDispatchSuccessfullyDispatchesValidRequest) {
  std::unique_ptr<NiceMock<MockHandlerFactory>> movable_mock_factory =
      std::make_unique<NiceMock<MockHandlerFactory>>();
  mock_factory_ = movable_mock_factory.get();
  registry_->RegisterRpc(1, std::move(movable_mock_factory));

  RawMessage msg{
      .rpc_id = 1,
      .payload_size = 10,
      .request_payload = "Grayson",
  };

  absl::AnyInvocable<void(std::string)> on_complete =
      [](std::string response_bytes) { return; };

  absl::Status response_status =
      registry_->DispatchRequest(std::move(msg), std::move(on_complete));
  ABSL_ASSERT_OK(response_status);
}

TEST_F(RpcRegistryTest, RpcRegistryDispatchReturnsErrorOnRpcFailure) {
  std::unique_ptr<NiceMock<MockHandlerFactory>> movable_mock_factory =
      std::make_unique<NiceMock<MockHandlerFactory>>();
  mock_factory_ = movable_mock_factory.get();
  registry_->RegisterRpc(1, std::move(movable_mock_factory));

  RawMessage msg{
      .rpc_id = 1,
      .payload_size = 10,
      .request_payload = "Grayson",
  };

  absl::AnyInvocable<void(std::string)> on_complete =
      [](std::string response_bytes) { return; };

  // have the factory return a mock handler passing "true" to the constructor
  // this will make the mock handler return an internal error when
  // HandleRequest is called
  EXPECT_CALL(*mock_factory_, CreateHandler(::testing::_, ::testing::_))
      .Times(1)
      .WillOnce(Return(std::make_unique<NiceMock<MockHandler>>(
          msg, std::move(on_complete), true)));

  absl::Status response_status =
      registry_->DispatchRequest(std::move(msg), std::move(on_complete));
  EXPECT_THAT(response_status,
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("A terrible error has occured")));
}
}  // namespace rpc