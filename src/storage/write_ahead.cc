#include "src/storage/write_ahead.h"

#include <unistd.h>

#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/kv_service/data.h"

namespace kvstore {
namespace {
absl::Status ValidateWriteRequest(const Request& request) {
  if (request.request_type == RequestType::GET) {
    return absl::InvalidArgumentError(
        "Request type is GET. Only PUT requests are logged");
  }

  if (!request.value.has_value()) {
    return absl::InvalidArgumentError("Request key is null.");
  }
  return absl::OkStatus();
}
}  // namespace

WriteAheadLog::WriteAheadLog(FILE* write_log, int log_fd)
    : log_fd_(log_fd), write_log_(write_log) {};

WriteAheadLog::~WriteAheadLog() {
  if (write_log_ != nullptr) {
    fclose(write_log_);
  }
}

absl::StatusOr<WriteAheadLog> WriteAheadLog::CreateLog(
    const std::string& log_file_path, const bool write_only) {
  const char* mode = write_only ? "a" : "+a";
  FILE* write_log = fopen(log_file_path.data(), mode);
  if (write_log == nullptr) {
    return absl::ErrnoToStatus(errno, "fopen failed");
  }
  int log_fd = fileno(write_log);
  if (log_fd < 1) {
    return absl::ErrnoToStatus(errno, "Error getting log fd.");
  }
  return WriteAheadLog(write_log, log_fd);
}

absl::Status WriteAheadLog::LogRequest(const Request& request) {
  if (write_log_ == nullptr) {
    return absl::InternalError("Log file is nullptr");
  }

  ABSL_RETURN_IF_ERROR(ValidateWriteRequest(request));

  std::string request_log = ConvertRequestToString(request);
  size_t bytes_written =
      fwrite(request_log.data(), sizeof(char), request_log.size(), write_log_);
  if (bytes_written < request_log.size()) {
    return absl::InternalError(absl::StrFormat(
        "Failed to write full log. Wrote %d bytes, but intended to write %d",
        bytes_written, request_log.size()));
  }
  if (fsync(log_fd_) < 0) {
    return absl::ErrnoToStatus(errno, "fsync failed.");
  }
  return absl::OkStatus();
}

absl::Status WriteAheadLog::Close() {
  int is_eof = 0;
  if (write_log_ != nullptr) {
    is_eof = fclose(write_log_);
  }
  write_log_ = nullptr;
  if (is_eof != 0) {
    return absl::ErrnoToStatus(errno, "fclose failed.");
  }
  return absl::OkStatus();
}

std::string WriteAheadLog::ConvertRequestToString(const Request& request) {
  std::string log = absl::StrCat("PUT:", request.key, ":", *request.value);
  return log;
}
}  // namespace kvstore