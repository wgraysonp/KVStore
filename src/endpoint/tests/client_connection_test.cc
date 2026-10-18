#include "src/endpoint/client_connection.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "src/rpc_framework/tests/mock_rpc_registry.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;
using ::testing::NiceMock;

namespace endpoint {

class ClientConnectionTest : public ::testing::Test {
 protected:
  int local_fd_ = -1;
  int remote_fd_ = -1;
  std::shared_ptr<NiceMock<rpc::MockRpcRegistry>> mock_registry_;
  std::unique_ptr<ClientConnection> client_connection_;

  void SetUp() override {
    int sv[2];
    ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0)
        << "socket pair failed";
    local_fd_ = sv[0];
    remote_fd_ = sv[1];
    mock_registry_ = std::make_shared<NiceMock<rpc::MockRpcRegistry>>();
    client_connection_ =
        std::make_unique<ClientConnection>(remote_fd_, mock_registry_);
  }

  void TearDown() override {
    // client connection destructor should close remote_fd_
    if (local_fd_ != -1) close(local_fd_);
  }
};

TEST_F(ClientConnectionTest, MakeNonBlockingCorrectlySetsNonBlockingFlag) {
  ABSL_EXPECT_OK(client_connection_->MakeNonBlocking());
  int flags = fcntl(remote_fd_, F_GETFL, 0);

  ASSERT_GE(flags, 0);

  EXPECT_EQ(flags & O_NONBLOCK, O_NONBLOCK);
}

TEST_F(ClientConnectionTest, HandleEventFailsIfReadFromClientReturnsError) {
  // send some data from the server fd to the client fd
  int bytes_sent = send(remote_fd_, "Half Data", 9, 0);
  ASSERT_EQ(bytes_sent, 9);

  // close local_fd_ without reading the buffer to send RST to reciever.
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  // check that the test worked as intended and errno was set to ECONNRESET
  ASSERT_EQ(errno, ECONNRESET);

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kUnavailable,
                               HasSubstr("Connection reset by peer")));
}
}  // namespace endpoint