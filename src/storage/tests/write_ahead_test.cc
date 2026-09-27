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
using ::testing::ContainerEq;
using ::testing::HasSubstr;

namespace kvstore {

class WriteAheadLogTest : public ::testing::Test {
 protected:
  std::unique_ptr<WriteAheadLog> logger_;
  std::filesystem::path test_directory_;
  std::filesystem::path test_wal_path_;
  std::ifstream log_file_;

  void SetUp() override {
    test_directory_ = std::filesystem::temp_directory_path() /
                      ("wal_tests" + std::to_string(rand()));
    ASSERT_TRUE(std::filesystem::create_directory(test_directory_));
    test_wal_path_ = test_directory_ / "kvstore_wal_test.log";

    absl::StatusOr<std::unique_ptr<WriteAheadLog>> log_status =
        WriteAheadLog::CreateLog(test_wal_path_.string());

    ABSL_ASSERT_OK(log_status);

    log_file_ = std::ifstream(test_wal_path_.string());
    ASSERT_TRUE(log_file_.is_open());

    logger_ = std::move(log_status.value());
  }

  void TearDown() override {
    ABSL_EXPECT_OK(logger_->Close());
    log_file_.close();
    std::error_code ec;
    std::filesystem::remove_all(test_directory_, ec);
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

  std::string expected_line = "test_key:test_value";

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

  std::string expected_line1 = "test_key1:test_value1";
  std::string expected_line2 = "test_key2:test_value2";

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

TEST_F(WriteAheadLogTest, RecoveryReturnsEmptyHashMapWithEmptyLog) {
  // Test file should exist but is empty
  absl::StatusOr<absl::flat_hash_map<std::string, std::string>> map_status =
      logger_->RecoverKVStore();

  ABSL_ASSERT_OK(map_status);

  const auto key_value_map = std::move(map_status.value());

  EXPECT_EQ(key_value_map.size(), 0);
}

TEST_F(WriteAheadLogTest, RecoverySuccessfullyRecoversEntriesAfterCrash) {
  // Write some requests to the log
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

  std::string expected_line1 = "test_key1:test_value1";
  std::string expected_line2 = "test_key2:test_value2";

  ABSL_EXPECT_OK(logger_->LogRequest(request1));
  ABSL_EXPECT_OK(logger_->LogRequest(request2));

  // destroy the logger class to simulate a crash
  logger_.reset();

  // reconstruct the class to simulate reboot
  absl::StatusOr<std::unique_ptr<WriteAheadLog>> log_status =
      WriteAheadLog::CreateLog(test_wal_path_.string());

  ABSL_ASSERT_OK(log_status);

  logger_ = std::move(log_status.value());

  // try to recover the kv map
  absl::StatusOr<absl::flat_hash_map<std::string, std::string>> map_status =
      logger_->RecoverKVStore();

  ABSL_ASSERT_OK(map_status);

  const auto recovered_map = std::move(map_status.value());

  absl::flat_hash_map<std::string, std::string> expected_map = {
      {"test_key1", "test_value1"}, {"test_key2", "test_value2"}};

  EXPECT_THAT(recovered_map, ContainerEq(expected_map));
}

}  // namespace kvstore
