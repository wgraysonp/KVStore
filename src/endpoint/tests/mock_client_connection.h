#ifndef ENDPOINT_TESTS_MOCK_CLIENT_CONNECTION_H_
#define ENDPOINT_TESTS_MOCK_CLIENT_CONNECTION_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/endpoint/client_connection.h"

namespace endpoint {

class MockClientConnection : public IClientConnection {
 public:
  MockClientConnection(DisconnectCallback on_disconnect)
      : on_disconnect_(std::move(on_disconnect)) {}

  MOCK_METHOD((void), MakeNonBlocking, (), (override));
  MOCK_METHOD((void), HandleEvent, (), (override));
  MOCK_METHOD((void), TimeOut, (), (override));

  void RunDisconnectCallback() {
    if (on_disconnect_) on_disconnect_();
  }

 private:
  DisconnectCallback on_disconnect_;
};

class MockClientConnectionFactory : public IClientConnectionFactory {
 public:
  MockClientConnectionFactory() {
    ON_CALL(*this,
            CreateClientConnection(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](int client_fd,
                          std::shared_ptr<const rpc::IRpcRegistry> rpc_registry,
                          DisconnectCallback on_disconnect) {
          auto mock_connection =
              std::make_shared<MockClientConnection>(std::move(on_disconnect));
          return mock_connection;
        });
  }

  MOCK_METHOD((std::shared_ptr<IClientConnection>), CreateClientConnection,
              (int client_fd,
               std::shared_ptr<const rpc::IRpcRegistry> rpc_registry,
               DisconnectCallback on_disconnect),
              (override));
};
}  // namespace endpoint

#endif  // ENDPOINT_TESTS_MOCK_CLIENT_CONNECTION_H_