#include "src/storage/write_ahead.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_format.h"
#include "src/kv_service/data.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

namespace kvstore {

class WriteAheadLogTest : public ::testing::Test {
 protected:
  std::unique_ptr<WriteAheadLog> logger_;
  std::filesystem::path test_path_;
  std::ifstream log_file_;

  void SetUp() override {
    test_path_ = std::filesystem::temp_directory_path() /
                 ("wal_tests" + std::to_string(rand()));
    ASSERT_TRUE(std::filesystem::create_directory(test_path_));
    std::filesystem::path wal_path = test_path_ / "kvstore_wal_test.log";

    absl::StatusOr<std::unique_ptr<WriteAheadLog>> log_status =
        WriteAheadLog::CreateLog(wal_path.string());

    ABSL_ASSERT_OK(log_status);

    log_file_ = std::ifstream(wal_path.string());
    ASSERT_TRUE(log_file_.is_open());

    logger_ = std::move(log_status.value());
  }

  void TearDown() override {
    ABSL_EXPECT_OK(logger_->Close());
    log_file_.close();
    std::error_code ec;
    std::filesystem::remove_all(test_path_, ec);
  }
};

TEST_F(WriteAheadLogTest, LogIsConstructedWithoutErrors) { SUCCEED(); }

TEST_F(WriteAheadLogTest, GetRequestsAreNotLogged) {
  Request request{};
  request.request_type = RequestType::GET;
  request.key = "test_key";

  absl::Status status = logger_->LogRequest(request);

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kInvalidArgument,
                               HasSubstr("Request type is GET")));
}

TEST_F(WriteAheadLogTest, PutRequestsWithNullKeyReturnError) {
  Request request{};
  request.request_type = RequestType::PUT;
  request.key = "test_key";

  absl::Status status = logger_->LogRequest(request);

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kInvalidArgument,
                               HasSubstr("value is null")));
}

TEST_F(WriteAheadLogTest, SingleRequestIsLoggesCorrectly) {
  Request request{
      .request_type = RequestType::PUT,
      .key = "test_key",
      .value = "test_value",
  };

  std::string expected_line = "PUT:test_key:test_value";

  ABSL_EXPECT_OK(logger_->LogRequest(request));

  log_file_.clear();
  std::string logged_line;
  EXPECT_TRUE(std::getline(log_file_, logged_line));
  EXPECT_EQ(logged_line, expected_line);

  EXPECT_EQ(log_file_.peek(), std::char_traits<char>::eof());
}

TEST_F(WriteAheadLogTest, MultipleRequestsLoggedCorrectly) {
  Request request1{
      .request_type = RequestType::PUT,
      .key = "test_key1",
      .value = "test_value1",
  };

  Request request2{
      .request_type = RequestType::PUT,
      .key = "test_key2",
      .value = "test_value2",
  };

  std::string expected_line1 = "PUT:test_key1:test_value1";
  std::string expected_line2 = "PUT:test_key2:test_value2";

  ABSL_EXPECT_OK(logger_->LogRequest(request1));
  ABSL_EXPECT_OK(logger_->LogRequest(request2));

  log_file_.clear();
  std::string logged_line1;
  EXPECT_TRUE(std::getline(log_file_, logged_line1));
  EXPECT_EQ(logged_line1, expected_line1);

  std::string logged_line2;
  EXPECT_TRUE(std::getline(log_file_, logged_line2));
  EXPECT_EQ(logged_line2, expected_line2);

  EXPECT_EQ(log_file_.peek(), std::char_traits<char>::eof());
}

}  // namespace kvstore
