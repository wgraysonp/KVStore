#include "src/rpc_framework/rpc_registry.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
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
  absl::StatusOr<std::string> response_bytes = registry_->DispatchRequest(msg);
  EXPECT_THAT(response_bytes.status(),
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

  absl::StatusOr<std::string> response_bytes = registry_->DispatchRequest(msg);
  ABSL_ASSERT_OK(response_bytes);

  EXPECT_EQ(response_bytes.value(), "Hello Grayson");
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

  // have the factory return a mock handler passing "true" to the constructor
  // this will make the mock handler return an internal error when
  // HandleRequest is called
  EXPECT_CALL(*mock_factory_, CreateHandler())
      .Times(1)
      .WillOnce(Return(std::make_unique<NiceMock<MockHandler>>(true)));

  absl::StatusOr<std::string> response_bytes = registry_->DispatchRequest(msg);
  EXPECT_THAT(response_bytes.status(),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("A terrible error has occured")));
}
}  // namespace rpc