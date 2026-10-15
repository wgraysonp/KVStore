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
  int sv[2] = {-1, -1};
  int local_fd_ = -1;
  int remote_fd_ = -1;

  void SetUp() override {
    ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0)
        << "socket pair failed";
    local_fd_ = sv[0];
    remote_fd_ = sv[1];
  }

  void TearDown() override {
    if (sv[0] != -1) close(sv[0]);
    if (sv[1] != -1) close(sv[1]);
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

TEST_F(SocketUtilsTest,
       ReadFromClientReturnsEOFStatusUponClientCloseAndZeroByteRead) {
  close(local_fd_);

  // set local_fd_ to -1 to prevent double close in TearDown
  local_fd_ = -1;

  std::string buffer('\0', 5);

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

  std::string buffer('\0', 18);

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

  std::string buffer('\0', 10);

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

  std::string buffer('\0', 10);
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

  std::string buffer('\0', 9);
  // read all the bytes from the client
  absl::StatusOr<ReadResult> result = ReadFromClient(buffer, 0, 9, remote_fd_);

  ABSL_ASSERT_OK(result);

  EXPECT_EQ(result.value().bytes_read, (std::size_t)9);
  EXPECT_EQ(result.value().state, ReadState::kSUCCESS);
  EXPECT_EQ(buffer, "Full Data");
}

}  // namespace endpoint