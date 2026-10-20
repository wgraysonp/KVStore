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
#include "src/endpoint/consts.h"
#include "src/rpc_framework/tests/mock_rpc_registry.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;

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

TEST_F(ClientConnectionTest, HandleEventSuccessfullyHandlesFullEventCycle) {
  std::string payload = "Grayson";
  uint32_t payload_size = htonl(static_cast<uint32_t>(payload.size()));
  uint32_t rpc_id = htonl(1);
  char size_header[4];
  char rpc_id_header[4];
  std::memcpy(&size_header, &payload_size, 4);
  std::memcpy(&rpc_id_header, &rpc_id, 4);

  // send the size and rpc id headers and client payload
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, &rpc_id_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, payload.data(), payload.size(), 0), 7);

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();

  // we should still be connected and ready to read
  EXPECT_EQ(status.value(), ConnectionStatus::CONNECTED_READ);

  // the mock rpc registry should return this to client_connection_, which
  // shoud attempt to send it to the client
  std::string expected_response = "Hello Grayson";

  std::string response_header_buffer;
  response_header_buffer.resize(4);
  std::string client_buffer;
  client_buffer.resize(expected_response.size());

  // HandleEvent should have written back to client. Read the header, then read
  // 13 bytes, which is the size of expected response
  ASSERT_EQ(recv(local_fd_, response_header_buffer.data(), 4, 0), 4);
  ASSERT_EQ(recv(local_fd_, client_buffer.data(), expected_response.size(), 0),
            13);

  uint32_t header;
  std::memcpy(&header, response_header_buffer.data(), 4);
  EXPECT_EQ(ntohl(header), (uint32_t)13);
  EXPECT_EQ(client_buffer, expected_response);

  // close the local_fd_ to send a FIN packet.
  close(local_fd_);
  local_fd_ = -1;

  status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_SUCCESS);
}

TEST_F(ClientConnectionTest,
       HandleEventSuccessfullyResumesAndHandlesEventWhenSentPartialPayload) {
  // here we mimic the case that the client sends the full payload "Grayson",
  // but the read is interupted by EAGAIN.

  ABSL_ASSERT_OK(client_connection_->MakeNonBlocking());

  std::string half_payload = "Gray";

  // we promise to send 7 bytes for the full payload "Grayson"
  uint32_t payload_size = htonl(7);
  uint32_t rpc_id = htonl(1);
  char size_header[4];
  char rpc_id_header[4];
  std::memcpy(&size_header, &payload_size, 4);
  std::memcpy(&rpc_id_header, &rpc_id, 4);

  // send the size and rpc id headers and partial client payload
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, &rpc_id_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, half_payload.data(), half_payload.size(), 0), 4);

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();

  // we should still be connected and ready to read
  EXPECT_EQ(status.value(), ConnectionStatus::CONNECTED_READ);

  // send the remaining portion of the payload to mimic remaining packets
  // arriving after EAGAIN
  std::string second_half_payload = "son";
  ASSERT_EQ(send(local_fd_, second_half_payload.data(),
                 second_half_payload.size(), 0),
            3);

  // handle the new event
  status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();

  // the mock rpc registry should return this to client_connection_, which
  // shoud attempt to send it to the client
  std::string expected_response = "Hello Grayson";

  std::string response_header_buffer;
  response_header_buffer.resize(4);
  std::string client_buffer;
  client_buffer.resize(expected_response.size());

  // HandleEvent should have written back to client. Read the header, then read
  // 13 bytes, which is the size of expected response
  ASSERT_EQ(recv(local_fd_, response_header_buffer.data(), 4, 0), 4);
  ASSERT_EQ(recv(local_fd_, client_buffer.data(), expected_response.size(), 0),
            13);

  uint32_t header;
  std::memcpy(&header, response_header_buffer.data(), 4);
  EXPECT_EQ(ntohl(header), (uint32_t)13);
  EXPECT_EQ(client_buffer, expected_response);

  // close the local_fd_ to send a FIN packet.
  close(local_fd_);
  local_fd_ = -1;

  status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_SUCCESS);
}

TEST_F(ClientConnectionTest,
       HandleEventReturnsDisconnectErrorIfClientConnectionFails) {
  // send some data from the server fd to the client fd
  int bytes_sent = send(remote_fd_, "Half Data", 9, 0);
  ASSERT_EQ(bytes_sent, 9);

  // close local_fd_ without reading the buffer to send RST to reciever.
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  // check that the test worked as intended and errno was set to ECONNRESET
  ASSERT_EQ(errno, ECONNRESET);

  ABSL_EXPECT_OK(status);

  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_ERROR);
}

TEST_F(ClientConnectionTest,
       HandleEventWritesInvalidArgumentToClientIfRequestedPayloadIsTooLarge) {
  uint32_t big_payload_size =
      htonl(static_cast<uint32_t>(MAX_CLIENT_PAYLOAD_SIZE) + 1);
  uint32_t rpc_id = htonl(1);
  char size_header[4];
  char rpc_id_header[4];
  std::memcpy(&size_header, &big_payload_size, 4);
  std::memcpy(&rpc_id_header, &rpc_id, 4);

  // send the size and rpc id headers. HandleClient event should enter
  // PerformClientRead(). after reading the rpc id, it will first check the size
  // header before reading the payload and should fail
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, &rpc_id_header, 4, 0), 4);

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();
  ABSL_ASSERT_OK(status);

  std::string expected_response =
      "INVALID_ARGUMENT: Requested payload is too large";

  std::string response_header_buffer;
  response_header_buffer.resize(4);
  std::string client_buffer;
  client_buffer.resize(expected_response.size());

  // HandleEvent should have written back to client. Read the header, then read
  ASSERT_EQ(recv(local_fd_, response_header_buffer.data(), 4, 0), 4);

  uint32_t header;
  std::memcpy(&header, response_header_buffer.data(), 4);
  EXPECT_EQ(ntohl(header), static_cast<uint32_t>(expected_response.size()));

  int payload_bytes_received =
      recv(local_fd_, client_buffer.data(), expected_response.size(), 0);
  EXPECT_EQ(payload_bytes_received, static_cast<int>(expected_response.size()));

  EXPECT_EQ(client_buffer, expected_response);
}

TEST_F(
    ClientConnectionTest,
    HandleEventReturnsSuccessfulDisconnectIfFINPacketIsRecievedAtStartOfRead) {
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();
  ABSL_ASSERT_OK(status);
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_SUCCESS);
}

TEST_F(
    ClientConnectionTest,
    HandleEventReturnsDisconnectErrorWhenClientClosesAfterSendingSizeHeader) {
  uint32_t payload_size = htonl(10);
  char size_header[4];
  std::memcpy(&size_header, &payload_size, 4);

  // send the size header then close the connection early
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status);
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_ERROR);
}

TEST_F(ClientConnectionTest,
       HandleEventReturnsDisconnectErrorWhenClientClosesAfterSendingRPCId) {
  uint32_t payload_size = htonl(10);
  uint32_t rpc_id = htonl(1);
  char size_header[4];
  std::memcpy(&size_header, &payload_size, 4);
  char rpc_id_header[4];
  std::memcpy(&rpc_id_header, &rpc_id, 4);

  // send the size header and rpc_id then close the connection early
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, &rpc_id_header, 4, 0), 4);
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status);
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_ERROR);
}

TEST_F(ClientConnectionTest, ClientConnectionDisconnectsIfRPCFails) {
  std::string payload = "Invalid Payload";
  uint32_t payload_size = htonl(static_cast<uint32_t>(payload.size()));
  uint32_t rpc_id = htonl(1);
  char size_header[4];
  char rpc_id_header[4];
  std::memcpy(&size_header, &payload_size, 4);
  std::memcpy(&rpc_id_header, &rpc_id, 4);

  // send the size and rpc id headers and client payload
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, &rpc_id_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, payload.data(), payload.size(), 0), 15);

  EXPECT_CALL(*mock_registry_, DispatchRequest(::testing::_))
      .Times(1)
      .WillOnce(Return(absl::InvalidArgumentError("Bad rpc request data")));

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();

  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_ERROR);

  std::string expected_response = "INVALID_ARGUMENT: Bad rpc request data";

  std::string response_header_buffer;
  response_header_buffer.resize(4);
  std::string client_buffer;
  client_buffer.resize(expected_response.size());

  // HandleEvent should have written back to client. Read the header, then read
  ASSERT_EQ(recv(local_fd_, response_header_buffer.data(), 4, 0), 4);

  uint32_t header;
  std::memcpy(&header, response_header_buffer.data(), 4);
  EXPECT_EQ(ntohl(header), static_cast<uint32_t>(expected_response.size()));

  int payload_bytes_received =
      recv(local_fd_, client_buffer.data(), expected_response.size(), 0);
  EXPECT_EQ(payload_bytes_received, static_cast<int>(expected_response.size()));

  EXPECT_EQ(client_buffer, expected_response);
}

TEST_F(ClientConnectionTest,
       ClientConnectionDisconnectsOnAttemptToWriteToBrokenPipe) {
  std::string payload = "Payload";
  uint32_t payload_size = htonl(static_cast<uint32_t>(payload.size()));
  uint32_t rpc_id = htonl(1);
  char size_header[4];
  char rpc_id_header[4];
  std::memcpy(&size_header, &payload_size, 4);
  std::memcpy(&rpc_id_header, &rpc_id, 4);

  // send the size and rpc id headers and client payload
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, &rpc_id_header, 4, 0), 4);
  ASSERT_EQ(send(local_fd_, payload.data(), payload.size(), 0), 7);

  close(local_fd_);
  local_fd_ = -1;


  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status) << "Expected Ok client connection status but got "
                         << status.status().ToString();

  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_ERROR);
}

TEST_F(ClientConnectionTest,
       HandlEventFailsIfConnectionIsDisconnectedSuccessfully) {
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status);
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_SUCCESS);

  absl::StatusOr<ConnectionStatus> status2 = client_connection_->HandleEvent();
  EXPECT_THAT(status2,
              StatusIs(absl::StatusCode::kFailedPrecondition,
                       HasSubstr("ClientConnection is in a disconnected state "
                                 "and should be deleted.")));
}

TEST_F(ClientConnectionTest, HandlEventFailsIfConnectionIsDisconnectedinError) {
  uint32_t payload_size = htonl(10);
  char size_header[4];
  std::memcpy(&size_header, &payload_size, 4);

  // send the size header and rpc_id then close the connection early
  ASSERT_EQ(send(local_fd_, &size_header, 4, 0), 4);
  close(local_fd_);
  local_fd_ = -1;

  absl::StatusOr<ConnectionStatus> status = client_connection_->HandleEvent();

  ABSL_ASSERT_OK(status);
  EXPECT_EQ(status.value(), ConnectionStatus::DISCONNECTED_ERROR);

  // the file descriptor should be closed and invalid at this point
  absl::StatusOr<ConnectionStatus> status2 = client_connection_->HandleEvent();
  EXPECT_THAT(status2,
              StatusIs(absl::StatusCode::kFailedPrecondition,
                       HasSubstr("Client file descriptor is invalid")));
}
}  // namespace endpoint