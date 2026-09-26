#include "src/storage/write_ahead.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_format.h"

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

namespace kvstore {

class WriteAheadLogTest : public ::testing::Test {
 protected:
  std::unique_ptr<WriteAheadLog> log_;
  std::filesystem::path test_path_;

  void SetUp() override {
    test_path_ = std::filesystem::temp_directory_path() /
                 ("wal_tests" + std::to_string(rand()));
    ASSERT_TRUE(std::filesystem::create_directory(test_path_));
    std::filesystem::path wal_path = test_path_ / "kvstore_wal_test.log";

    absl::StatusOr<std::unique_ptr<WriteAheadLog>> log_status =
        WriteAheadLog::CreateLog(wal_path.string(), false);

    ABSL_ASSERT_OK(log_status);

    log_ = std::move(log_status.value());
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(test_path_, ec);
  }
};

TEST_F(WriteAheadLogTest, LogIsConstructedWithoutErrors) { SUCCEED(); }

}  // namespace kvstore
