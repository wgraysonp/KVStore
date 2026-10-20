#include "src/endpoint/socket_utils.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstddef>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

namespace endpoint {

class SocketUtilsTest : public ::testing::Test {
 protected:
  int local_fd_ = -1;
  int remote_fd_ = -1;

  void SetUp() override {
    int sv[2];
    ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0)
        << "socket pair failed";
    local_fd_ = sv[0];
    remote_fd_ = sv[1];
  }

  void TearDown() override {
    if (local_fd_ != -1) close(local_fd_);
    if (remote_fd_ != -1) close(remote_fd_);
  }
};

TEST_F(SocketUtilsTest, SetNonBlockingFailsWithInvalidFD) {
  int fd = -1;
  absl::Status status = SetNonBlocking(fd);
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kFailedPrecondition,
                               HasSubstr("Bad file descriptor")));
}

TEST_F(SocketUtilsTest, SetNonBlockingCorrectlySetsNonBlockingFlag) {
  ABSL_EXPECT_OK(SetNonBlocking(remote_fd_));
  int flags = fcntl(remote_fd_, F_GETFL, 0);

  ASSERT_GE(flags, 0);

  EXPECT_EQ(flags & O_NONBLOCK, O_NONBLOCK);
}

TEST_F(SocketUtilsTest, ReadUint32FailsIfPassedABufferWithLengthOtherThan4) {
  uint64_t big_num = 88873;
  char* num_bytes = reinterpret_cast<char*>(&big_num);
  auto test_big_num = ReadUint32(std::string(num_bytes, num_bytes + 8));
  EXPECT_THAT(test_big_num,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Buffer must be exactly four bytes.")));

  std::string small_num = "0x3";
  auto test_small_num = ReadUint32(small_num);
  EXPECT_THAT(test_small_num,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Buffer must be exactly four bytes.")));
}

TEST_F(SocketUtilsTest, ReadUint32CorrectlyReadsUint32) {
  uint32_t actual_num = htonl(75);
  char* num_bytes = reinterpret_cast<char*>(&actual_num);
  auto test_num = ReadUint32(std::string(num_bytes, num_bytes + 4));
  ABSL_ASSERT_OK(test_num);

  EXPECT_EQ(ntohl(actual_num), *test_num)
      << "Expected (hex): 0x" << std::hex << actual_num << ", but got: 0x"
      << test_num;
}

TEST_F(SocketUtilsTest, ConvertUint32ToNetworkBytesCorrectlyConvertUint32) {
  uint32_t expected_num = 75;
  std::string num_network_bytes = ConvertUint32ToNetworkBytes(expected_num);

  uint32_t actual_num;
  std::memcpy(&actual_num, num_network_bytes.data(), sizeof(uint32_t));
  EXPECT_EQ(expected_num, ntohl(actual_num));
}

TEST_F(SocketUtilsTest,
       ReadFromClientReturnsEOFStatusUponClientCloseAndZeroByteRead) {
  close(local_fd_);

  // set local_fd_ to -1 to prevent double close in TearDown
  local_fd_ = -1;

  std::string buffer(5, '\0');

  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 0, 5, remote_fd_);

  ABSL_ASSERT_OK(result);
  EXPECT_EQ(result.value().bytes_read, (std::size_t)0);
  EXPECT_EQ(result.value().state, ReadState::kEOF);
  EXPECT_EQ(buffer, "");
}

TEST_F(SocketUtilsTest, ReadFromClientReturnsErrorOnMidReadClientClose) {
  int bytes_sent = send(local_fd_, "Half Data", 9, 0);
  ASSERT_EQ(bytes_sent, 9);

  close(local_fd_);
  local_fd_ = -1;

  std::string buffer(18, '\0');

  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 0, 18, remote_fd_);

  EXPECT_THAT(result,
              StatusIs(absl::StatusCode::kCancelled,
                       HasSubstr("closed connection cleanly mid read.")));
  EXPECT_EQ(buffer, "");
}

TEST_F(SocketUtilsTest, ReadFromClientReturnsErrorIfRSTIsRecieved) {
  // send some data from the server fd to the client fd
  int bytes_sent = send(remote_fd_, "Half Data", 9, 0);
  ASSERT_EQ(bytes_sent, 9);

  // close local_fd_ without reading the buffer to send RST to reciever.
  close(local_fd_);
  local_fd_ = -1;

  std::string buffer(10, '\0');

  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 0, 10, remote_fd_);

  // check that the test worked as intended and errno was set to ECONNRESET
  ASSERT_EQ(errno, ECONNRESET);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnavailable,
                               HasSubstr("Connection reset by peer")));
  EXPECT_EQ(buffer, "");
}

// tests that if the server reads partial data without a closed connection
// and the socket is non-blocking then ReadFromClient returns with an
// appropriate ReadStatus code
TEST_F(SocketUtilsTest, ReadFromClientReturnsEAGAINonValidPartialRead) {
  ABSL_ASSERT_OK(SetNonBlocking(remote_fd_));

  // send some data from the server fd to the client fd
  int bytes_sent = send(local_fd_, "Half Data", 9, 0);
  ASSERT_EQ(bytes_sent, 9);

  // keep the local file descriptor open

  std::string buffer(10, '\0');
  // attempt to read more bytes than the client has sent
  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 0, 10, remote_fd_);

  ABSL_ASSERT_OK(result);

  // check that the test worked as intended and errno was set to EAGAIN
  ASSERT_EQ(errno, EAGAIN);

  EXPECT_EQ(result.value().bytes_read, (std::size_t)9);
  EXPECT_EQ(result.value().state, ReadState::kEAGAIN);
  EXPECT_EQ(buffer, "Half Data");
}

TEST_F(SocketUtilsTest, ReadFromClientCorrectlyPerformsFullRead) {
  ABSL_ASSERT_OK(SetNonBlocking(remote_fd_));

  // send all the data to the server
  int bytes_sent = send(local_fd_, "Full Data", 9, 0);
  ASSERT_EQ(bytes_sent, 9);

  std::string buffer(9, '\0');
  // read all the bytes from the client
  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 0, 9, remote_fd_);

  ABSL_ASSERT_OK(result);

  EXPECT_EQ(result.value().bytes_read, (std::size_t)9);
  EXPECT_EQ(result.value().state, ReadState::kSUCCESS);
  EXPECT_EQ(buffer, "Full Data");
}

TEST_F(SocketUtilsTest,
       ReadFromClientCorrectlyPerformsFullReadIntoPartiallyFullBuffer) {
  ABSL_ASSERT_OK(SetNonBlocking(remote_fd_));

  // send only "Data" to the server. We simulate "Full " having already been
  // read in a previous call
  int bytes_sent = send(local_fd_, "Data", 4, 0);
  ASSERT_EQ(bytes_sent, 4);

  std::string buffer = "Full ";
  // read all the bytes from the client
  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 5, 4, remote_fd_);

  ABSL_ASSERT_OK(result);

  EXPECT_EQ(result.value().bytes_read, (std::size_t)4);
  EXPECT_EQ(result.value().state, ReadState::kSUCCESS);
  EXPECT_EQ(buffer, "Full Data");
}

TEST_F(SocketUtilsTest, WriteToClientReturnsErrorIfBufferTooSmall) {
  std::string buffer(10, 'A');

  // offset 5 + bytes to write 6 is too large
  absl::StatusOr<WriteResult> result = WriteToClient(buffer, 5, 6, remote_fd_);

  EXPECT_THAT(
      result,
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Buffer size is too small for requested write")));
}

TEST_F(SocketUtilsTest, WriteToClientFailsOnSendToBrokenPipeButDoesNotCrash) {
  // close the client file descriptor
  close(local_fd_);
  local_fd_ = -1;

  std::string buffer = "Hello";

  // attempt to send the message. This should set fail and set errno to EPIPE
  absl::StatusOr<WriteResult> result = WriteToClient(buffer, 0, 5, remote_fd_);

  // if we make it to this assert statement, it also verifies that SIGPIPE
  // was correctly intercepted instead of killing the process
  ASSERT_EQ(errno, EPIPE);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kFailedPrecondition,
                               HasSubstr("Broken pipe")));
}

TEST_F(SocketUtilsTest, WriteToClientReturnsCorrectCodeOnEAGAIN) {
  ABSL_ASSERT_OK(SetNonBlocking(remote_fd_));

  absl::StatusOr<WriteResult> result;
  std::size_t write_size = 1024 * 64;
  int max_attempts = 10;
  int n_attempts = 0;

  // write to the send buffer in a loop until it is full
  while (n_attempts++ < max_attempts) {
    std::string buffer(write_size, 'a');  // 64 KB
    result = WriteToClient(buffer, 0, write_size, remote_fd_);
    ABSL_ASSERT_OK(result);
    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
  }
  if (n_attempts >= max_attempts) {
    FAIL() << "Hit max attempts before errno was set to EAGAIN";
  }
  ASSERT_EQ(result.value().state, WriteState::kEAGAIN);
  ASSERT_LE(result.value().bytes_written, write_size);
}

TEST_F(SocketUtilsTest, WriteToClientReturnsSuccessfullyIfZeroBytesAreWritten) {
  std::string empty = "";
  auto res = WriteToClient(empty, 0, 0, remote_fd_);
  ABSL_ASSERT_OK(res);
  EXPECT_EQ(res.value().bytes_written, (std::size_t)0);
  EXPECT_EQ(res.value().state, WriteState::kSUCCESS);
}

TEST_F(SocketUtilsTest, WriteToClientPerformsFullWriteSuccessfully) {
  std::string good_write = "Full Write";
  absl::StatusOr<WriteResult> result =
      WriteToClient(good_write, 0, 10, remote_fd_);
  ABSL_ASSERT_OK(result);
  EXPECT_EQ(result.value().bytes_written, (std::size_t)10);
  EXPECT_EQ(result.value().state, WriteState::kSUCCESS);

  std::string client_buf;
  client_buf.resize(10);
  ASSERT_EQ(recv(local_fd_, client_buf.data(), 10, 0), 10);
  EXPECT_EQ(client_buf, "Full Write");
}

TEST_F(SocketUtilsTest,
       WriteToClientPerformsFullWriteSuccessfullyFromPositiveOffset) {
  std::string partial_write = "Old Data Write";
  absl::StatusOr<WriteResult> result =
      WriteToClient(partial_write, 9, 5, remote_fd_);
  ABSL_ASSERT_OK(result) << "Expected Ok but Got "
                         << result.status().ToString();
  EXPECT_EQ(result.value().bytes_written, (std::size_t)5);
  EXPECT_EQ(result.value().state, WriteState::kSUCCESS);

  std::string client_buf;
  client_buf.resize(5);
  ASSERT_EQ(recv(local_fd_, client_buf.data(), 5, 0), 5);
  EXPECT_EQ(client_buf, "Write");
}

TEST_F(SocketUtilsTest, WriteErrorMessageToClientCorrectlyWritesStringLiteral) {
  ABSL_ASSERT_OK(WriteErrorMessageToClient("Internal Error", remote_fd_));

  std::string expected_response = "Internal Error";

  std::string response_header_buffer;
  response_header_buffer.resize(4);
  std::string client_buffer;
  client_buffer.resize(expected_response.size());

  // HandleEvent should have written back to client. Read the header, then read
  // 13 bytes, which is the size of expected response
  ASSERT_EQ(recv(local_fd_, response_header_buffer.data(), 4, 0), 4);
  ASSERT_EQ(recv(local_fd_, client_buffer.data(), expected_response.size(), 0),
            14);

  uint32_t header;
  std::memcpy(&header, response_header_buffer.data(), 4);
  EXPECT_EQ(ntohl(header), (uint32_t)14);
  EXPECT_EQ(client_buffer, expected_response);
}

TEST_F(SocketUtilsTest, WriteErrorMessageToClientCorrectlyWritesAbslStatusCode) {
  absl::Status status = absl::InternalError("Horiffic error.");
  ABSL_ASSERT_OK(WriteErrorMessageToClient(status.ToString(), remote_fd_));

  std::string expected_response = "INTERNAL: Horiffic error.";

  std::string response_header_buffer;
  response_header_buffer.resize(4);
  std::string client_buffer;
  client_buffer.resize(expected_response.size());

  // HandleEvent should have written back to client. Read the header, then read
  // 13 bytes, which is the size of expected response
  ASSERT_EQ(recv(local_fd_, response_header_buffer.data(), 4, 0), 4);
  ASSERT_EQ(recv(local_fd_, client_buffer.data(), expected_response.size(), 0),
            25);

  uint32_t header;
  std::memcpy(&header, response_header_buffer.data(), 4);
  EXPECT_EQ(ntohl(header), (uint32_t)25);
  EXPECT_EQ(client_buffer, expected_response);
}

}  // namespace endpoint