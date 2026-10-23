#ifndef ENDPOINT_TESTS_MOCK_CLIENT_CONNECTION_H_
#define ENDPOINT_TESTS_MOCK_CLIENT_CONNECTION_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/endpoint/client_connection.h"

namespace endpoint {

class MockClientConnection : public IClientConnection {
 public:
  MockClientConnection() {
    ON_CALL(*this, MakeNonBlocking(::testing::_))
        .WillByDefault(::testing::Return(absl::OkStatus()));
  }

  MOCK_METHOD((absl::Status), MakeNonBlocking, (), (override));
  MOCK_METHOD((absl::StatusOr<ConnectionStatus>), HandleEvent, (), (override));
};

class MockClientConnectionFactory : public IClientConnectionFactory {
 public:
  MockClientConnectionFactory() = default;

  MOCK_METHOD((std::unique_ptr<IClientConnection>), CreateClientConnection,
              (int client_fd,
               std::shared_ptr<const rpc::IRpcRegistry> rpc_registry),
              (override));
};
}  // namespace endpoint

#endif  // ENDPOINT_TESTS_MOCK_CLIENT_CONNECTION_H_