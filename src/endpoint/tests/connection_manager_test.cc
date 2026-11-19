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
#include "src/endpoint/timer_list.h"
#include "src/rpc_framework/tests/mock_rpc_registry.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;
using ::testing::NiceMock;

#define PORT 8080

namespace endpoint {

class ConnectionManagerTest : public ::testing::Test {
 protected:
  NiceMock<MockClientConnectionFactory>* mock_factory_ = nullptr;
  std::shared_ptr<ConnectionManager> connection_manager_;
  TimerList* timer_list_ = nullptr;

  void SetUp() override {
    auto temp_mock_factory =
        std::make_unique<NiceMock<MockClientConnectionFactory>>();
    mock_factory_ = temp_mock_factory.get();

    auto mock_registry = std::make_shared<rpc::MockRpcRegistry>();

    std::unique_ptr<TimerList> temp_timer_list =
        std::make_unique<TimerList>(absl::Seconds(3));
    timer_list_ = temp_timer_list.get();

    connection_manager_ = std::make_shared<ConnectionManager>(
        PORT, std::move(mock_registry), std::move(temp_mock_factory),
        std::move(temp_timer_list));
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
  const absl::flat_hash_map<int, std::shared_ptr<IClientConnection>>&
      connections_map = connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map.size(), (std::size_t)1);

  close(client_fd);
}

TEST_F(ConnectionManagerTest,
       ConnectionManagerDeletesClientConnectionWhenDisconnectCallbackIsRun) {
  ABSL_EXPECT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  std::shared_ptr<MockClientConnection> mock_connection;

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_, ::testing::_))
      .Times(1)
      .WillOnce([&mock_connection](
                    int fd,
                    std::shared_ptr<const rpc::IRpcRegistry> rpc_registry,
                    DisconnectCallback on_disconnect) {
        mock_connection =
            std::make_shared<MockClientConnection>(std::move(on_disconnect));
        return mock_connection;
      });

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  const absl::flat_hash_map<int, std::shared_ptr<IClientConnection>>&
      connections_map_pre_event = connection_manager_->GetActiveConnections();

  ASSERT_EQ(connections_map_pre_event.size(), (std::size_t)1);

  // send some arbitrary data to trigger an epoll event
  std::string some_data = "Hello";
  ASSERT_THAT(send(client_fd, some_data.data(), some_data.size(), 0),
              ::testing::Eq(some_data.size()));

  EXPECT_CALL(*mock_connection, HandleEvent)
      .Times(1)
      .WillOnce(
          [&mock_connection]() { mock_connection->RunDisconnectCallback(); });

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  const absl::flat_hash_map<int, std::shared_ptr<IClientConnection>>&
      connections_map_post_disconnect =
          connection_manager_->GetActiveConnections();

  EXPECT_EQ(connections_map_post_disconnect.size(), (std::size_t)0);

  close(client_fd);
}

TEST_F(ConnectionManagerTest,
       ConnectionManagerCorrectlyDeletesTimedOutConnections) {
  ABSL_ASSERT_OK(connection_manager_->BindAndListen());
  ClientConnect();

  std::shared_ptr<MockClientConnection> mock_connection;

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_, ::testing::_))
      .Times(1)
      .WillOnce([&mock_connection](
                    int fd,
                    std::shared_ptr<const rpc::IRpcRegistry> rpc_registry,
                    DisconnectCallback on_disconnect) {
        mock_connection =
            std::make_shared<MockClientConnection>(std::move(on_disconnect));
        return mock_connection;
      });

  // loop once to accept the connection
  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  ASSERT_EQ(sleep(3), (unsigned int)0);

  ClientConnect();

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_, ::testing::_))
      .Times(1);

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));

  EXPECT_CALL(*mock_connection, TimeOut())
      .Times(1)
      .WillOnce(
          [&mock_connection]() { mock_connection->RunDisconnectCallback(); });

  connection_manager_->CloseExpiredConnections(absl::Now());

  const absl::flat_hash_map<int, std::shared_ptr<IClientConnection>>&
      connections_map = connection_manager_->GetActiveConnections();

  // timeout is set to 3 seconds so first connection should have been deleted,
  // but not the second
  EXPECT_EQ(connections_map.size(), (std::size_t)1);
}

TEST_F(ConnectionManagerTest, ConnectionManagerRefreshesConnectionTimer) {
  ABSL_ASSERT_OK(connection_manager_->BindAndListen());
  int client_fd = ClientConnect();

  std::shared_ptr<MockClientConnection> mock_connection;

  EXPECT_CALL(*mock_factory_,
              CreateClientConnection(::testing::_, ::testing::_, ::testing::_))
      .Times(1)
      .WillOnce([&mock_connection](
                    int fd,
                    std::shared_ptr<const rpc::IRpcRegistry> rpc_registry,
                    DisconnectCallback on_disconnect) {
        mock_connection =
            std::make_shared<MockClientConnection>(std::move(on_disconnect));
        return mock_connection;
      });

  // loop once to accept the connection
  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  ASSERT_EQ(sleep(2), (unsigned int)0);

  // send some arbitrary data to trigger an epoll event
  // connection manager should refresh the timeout deadline for client_fd
  std::string header = "header";
  ASSERT_THAT(send(client_fd, header.data(), header.size(), 0),
              ::testing::Eq(header.size()));

  // Handle event returns CONNECTED_READ to simulate that the
  // transaction is not complete
  EXPECT_CALL(*mock_connection, HandleEvent()).Times(1);

  ABSL_ASSERT_OK(connection_manager_->LoopOnce(100));
  ASSERT_EQ(sleep(2), (unsigned int)0);

  // it has now been at least four seconds since connection. The client should
  // not be evicted though because it has only been two seconds since last
  // activity
  EXPECT_CALL(*mock_connection, TimeOut()).Times(0);
  connection_manager_->CloseExpiredConnections(absl::Now());

  const absl::flat_hash_map<int, std::shared_ptr<IClientConnection>>&
      connections_map = connection_manager_->GetActiveConnections();
  EXPECT_EQ(connections_map.size(), (std::size_t)1);
}
}  // namespace endpoint