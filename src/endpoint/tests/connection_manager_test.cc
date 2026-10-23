#include "src/endpoint/connection_manager.h"

#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "src/endpoint/tests/mock_client_connection.h"
#include "src/rpc_framework/tests/mock_rpc_registry.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;
using ::testing::NiceMock;

namespace endpoint {

class ConnectionManagerTest : public ::testing::Test {
 protected:
  NiceMock<MockClientConnectionFactory>* mock_factory_;
  std::unique_ptr<ConnectionManager> connection_manager_;

  void SetUp() override {
    auto temp_mock_factory =
        std::make_unique<NiceMock<MockClientConnectionFactory>>();
    mock_factory_ = temp_mock_factory.get();

    auto mock_registry = std::make_shared<rpc::MockRpcRegistry>();

    connection_manager_ = std::make_unique<ConnectionManager>(
        8080, std::move(mock_registry), std::move(temp_mock_factory));
  }
};

TEST_F(ConnectionManagerTest, Testing){
  SUCCEED();
}