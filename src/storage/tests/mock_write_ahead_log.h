#ifndef SRC_STORAGE_TESTS_MOCK_WRITE_AHEAD_LOG_H_
#define SRC_STORAGE_TESTS_MOCK_WRITE_AHEAD_LOG_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "src/kv_service/data.h"
#include "src/storage/write_ahead.h"

namespace kvstore {

class MockWriteAheadLog : public IWriteAheadLog {
 public:
  MockWriteAheadLog() {
    ON_CALL(*this, LogRequest(::testing::_))
        .WillByDefault(::testing::Return(absl::OkStatus()));
  }
  ~MockWriteAheadLog() override = default;

  MOCK_METHOD((absl::Status), LogRequest, (const Request& request), (override));
  MOCK_METHOD((absl::StatusOr<absl::flat_hash_map<std::string, std::string>>),
              RecoverKVStore, (), (override));
};
}  // namespace kvstore

#endif  // SRC_STORAGE_TESTS_MOCK_WRITE_AHEAD_LOG_H_