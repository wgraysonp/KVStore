#ifndef KVSTORE_SRC_STORAGE_WRITE_AHEAD_H_
#define KVSTORE_SRC_STORAGE_WRITE_AHEAD_H_

#include <cstdio>
#include <fstream>
#include <memory>

#include "absl/status/status.h"
#include "src/kv_service/data.h"

namespace kvstore {

class WriteAheadLog {
 public:
  static absl::StatusOr<std::unique_ptr<WriteAheadLog>> CreateLog(
      const std::string& log_file_path, const bool write_only);
  WriteAheadLog(const WriteAheadLog&) = delete;
  WriteAheadLog& operator=(const WriteAheadLog&) = delete;
  WriteAheadLog(WriteAheadLog&&) noexcept = default;
  WriteAheadLog& operator=(WriteAheadLog&&) noexcept = default;
  ~WriteAheadLog();

  absl::Status LogRequest(const Request& request);
  absl::Status Close();

 private:
  explicit WriteAheadLog(FILE* write_log, int log_fd_);
  std::string ConvertRequestToString(const Request& request);

  int log_fd_;
  FILE* write_log_;
};

}  // namespace kvstore

#endif  // KVSTORE_SRC_STORAGE_WRITE_AHEAD_H_