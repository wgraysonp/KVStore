#include "src/endpoint/socket_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

namespace endpoint {

  class SocketUtilsTest : public ::testing::Test{};

  TEST_F(SocketUtilsTest, SetNonBlockingFailsWithInvalidFD) {
  int fd = -1;
  absl::Status status = SetNonBlocking(fd);
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kFailedPrecondition,
                               HasSubstr("Bad file descriptor")));
}

} // namespace endpoint