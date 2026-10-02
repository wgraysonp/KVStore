#ifndef KVSTORE_SRC_STORAGE_WRITE_AHEAD_H_
#define KVSTORE_SRC_STORAGE_WRITE_AHEAD_H_

#include <cstdio>
#include <fstream>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "src/kv_service/data.h"

namespace kvstore {

class IWriteAheadLog {
 public:
  IWriteAheadLog() = default; 
  IWriteAheadLog(const IWriteAheadLog&) = delete;
  IWriteAheadLog& operator=(const IWriteAheadLog&) = delete;
  IWriteAheadLog(IWriteAheadLog&&) noexcept = default;
  IWriteAheadLog& operator=(IWriteAheadLog&&) noexcept = default;
  virtual ~IWriteAheadLog() = default;

  virtual absl::Status LogRequest(const Request& request) = 0;
  virtual absl::StatusOr<absl::flat_hash_map<std::string, std::string>>
  RecoverKVStore() = 0;
};

class WriteAheadLog : public IWriteAheadLog {
 public:
  static absl::StatusOr<std::unique_ptr<WriteAheadLog>> CreateLog(
      std::string log_file_path);
  ~WriteAheadLog() override;

  absl::Status LogRequest(const Request& request) override;
  absl::StatusOr<absl::flat_hash_map<std::string, std::string>>
  RecoverKVStore() override;
  absl::Status Close();

 private:
  explicit WriteAheadLog(int log_fd_, std::string log_file_path);
  std::string ConvertRequestToString(const Request& request);

  int log_fd_;
  std::string log_file_path_;
};

}  // namespace kvstore

#endif  // KVSTORE_SRC_STORAGE_WRITE_AHEAD_H_