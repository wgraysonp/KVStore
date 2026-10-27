#include "src/endpoint/connection_manager.h"

#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
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

#define PORT 8080

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
        PORT, std::move(mock_registry), std::move(temp_mock_factory));
  }

  int ClientConnect() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
      ADD_FAILURE() << "Failed to create client socket descriptor";
      return -1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <
        0) {
      ADD_FAILURE() << "Failed to connect client socket";
      return -1;
    }
    return client_fd;
  }
};

TEST_F(ConnectionManagerTest, ConnectionManagerBindsToPortCorrectly) {
  ABSL_EXPECT_OK(connection_manager_->BindAndListen());
}

TEST_F(ConnectionManagerTest, ConnectionManagerAcceptsSingleClient) {
  ABSL_ASSERT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map = connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map.size(), (std::size_t)1);

  close(client_fd);
}

TEST_F(ConnectionManagerTest,
       ConnectionManagerDeletesClientConnectionOnSuccessfulDisconnect) {
  ABSL_EXPECT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  std::unique_ptr<MockClientConnection> mock_connection_to_return =
      std::make_unique<MockClientConnection>();
  MockClientConnection* mock_connection = mock_connection_to_return.get();

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(std::move(mock_connection_to_return)));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_pre_event = connection_manager_->GetActiveConnections();

  ASSERT_EQ(connections_map_pre_event.size(), (std::size_t)1);

  // send some arbitrary data to trigger an epoll event
  std::string some_data = "Hello";
  ASSERT_THAT(send(client_fd, some_data.data(), some_data.size(), 0),
              ::testing::Eq(some_data.size()));

  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(ConnectionStatus::DISCONNECTED_SUCCESS));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_disconnect =
          connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map_post_disconnect.size(), (std::size_t)0);

  close(client_fd);
}

TEST_F(ConnectionManagerTest,
       ConnectionManagerDeletesClientConnectionOnDisconnectInError) {
  ABSL_EXPECT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  std::unique_ptr<MockClientConnection> mock_connection_to_return =
      std::make_unique<MockClientConnection>();
  MockClientConnection* mock_connection = mock_connection_to_return.get();

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(std::move(mock_connection_to_return)));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_pre_event = connection_manager_->GetActiveConnections();

  ASSERT_EQ(connections_map_pre_event.size(), (std::size_t)1);

  // send some arbitrary data to trigger an epoll event
  std::string some_data = "Bad Data";
  ASSERT_THAT(send(client_fd, some_data.data(), some_data.size(), 0),
              ::testing::Eq(some_data.size()));

  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(ConnectionStatus::DISCONNECTED_ERROR));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_disconnect =
          connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map_post_disconnect.size(), (std::size_t)0);

  close(client_fd);
}

TEST_F(ConnectionManagerTest,
       ConnectionManagerDeletesConnectionIfHandleConnectFails) {
  ABSL_EXPECT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  std::unique_ptr<MockClientConnection> mock_connection_to_return =
      std::make_unique<MockClientConnection>();
  MockClientConnection* mock_connection = mock_connection_to_return.get();

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(std::move(mock_connection_to_return)));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_pre_event = connection_manager_->GetActiveConnections();

  ASSERT_EQ(connections_map_pre_event.size(), (std::size_t)1);

  // send some arbitrary data to trigger an epoll event
  std::string some_data = "Data";
  ASSERT_THAT(send(client_fd, some_data.data(), some_data.size(), 0),
              ::testing::Eq(some_data.size()));

  // Handle event returns an error for this connection
  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(absl::InternalError("Connectoin Corrupted")));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_disconnect =
          connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map_post_disconnect.size(), (std::size_t)0);

  close(client_fd);
}

TEST_F(ConnectionManagerTest, LoopOnceDoesNotFailIfClientConnectionNotFound) {
  ABSL_ASSERT_OK(connection_manager_->BindAndListen());

  int client_fd = ClientConnect();

  std::unique_ptr<MockClientConnection> mock_connection_to_return =
      std::make_unique<MockClientConnection>();
  MockClientConnection* mock_connection = mock_connection_to_return.get();

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(std::move(mock_connection_to_return)));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map = connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map.size(), (std::size_t)1);

  // send some arbitrary data to trigger an epoll event
  std::string some_data = "Data";
  ASSERT_THAT(send(client_fd, some_data.data(), some_data.size(), 0),
              ::testing::Eq(some_data.size()));

  // Handle event returns an error for this connection. In this case,
  // this will cause connection manager to delete the pointer to the mock
  // connection object. However, the mock connection destructor does not
  // close the client file descriptor. So we can still trigger epoll events
  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(absl::InternalError("Connectoin Corrupted")));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_disconnect =
          connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map_post_disconnect.size(), (std::size_t)0);

  // send a FIN packet. this will trigger an epoll event, but the
  // MockClientConnection object will have been delete already
  close(client_fd);

  // the connection manager should see that the MockClientCOnnection object
  // associaeted with the event fd is not found. It will log a warning and
  // continue
  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
}

TEST_F(ConnectionManagerTest,
       ConnectionManagerMaintainsConnectionUntilDisconnectStatus) {
  ABSL_EXPECT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  std::unique_ptr<MockClientConnection> mock_connection_to_return =
      std::make_unique<MockClientConnection>();
  MockClientConnection* mock_connection = mock_connection_to_return.get();

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(std::move(mock_connection_to_return)));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_pre_event = connection_manager_->GetActiveConnections();

  ASSERT_EQ(connections_map_pre_event.size(), (std::size_t)1);

  // send some arbitrary data to trigger an epoll event
  // this is to mimic the case that only part of of the full
  // request data is sent (e.g. just the header)
  std::string header = "header";
  ASSERT_THAT(send(client_fd, header.data(), header.size(), 0),
              ::testing::Eq(header.size()));

  // Handle event returns CONNECTED_READ to simulate that the
  // transaction is not complete
  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(ConnectionStatus::CONNECTED_READ));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_header_read =
          connection_manager_->GetActiveConnections();

  // the connection should still be alive and we are back in the epoll loop
  EXPECT_EQ(connections_map_post_header_read.size(), (std::size_t)1);

  std::string payload = "rest of payload";
  ASSERT_THAT(send(client_fd, payload.data(), payload.size(), 0),
              ::testing::Eq(payload.size()));

  // Handle event returns CONNECTED_DISPATCH to simulate that the
  // we are waiting on the rpc to finish
  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(ConnectionStatus::CONNECTED_DISPATCH));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_payload_read =
          connection_manager_->GetActiveConnections();

  // the connection should still be alive and we are back in the epoll loop
  EXPECT_EQ(connections_map_post_payload_read.size(), (std::size_t)1);

  // this should send a FIN packet to signal the connection is complete and
  // trigger another EPOLL event
  close(client_fd);

  // signal to the manager that the connection is closed and we are done
  EXPECT_CALL(*mock_connection, HandleEvent())
      .Times(1)
      .WillOnce(::testing::Return(ConnectionStatus::DISCONNECTED_SUCCESS));

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::unique_ptr<IClientConnection>>&
      connections_map_post_close = connection_manager_->GetActiveConnections();

  // the connection should be deleted now.
  EXPECT_EQ(connections_map_post_close.size(), (std::size_t)0);
}
}  // namespace endpoint