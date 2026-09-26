#include "src/storage/write_ahead.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
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
    return absl::InvalidArgumentError("Request value is null.");
  }
  return absl::OkStatus();
}
}  // namespace

WriteAheadLog::WriteAheadLog(int log_fd) : log_fd_(log_fd) {};

WriteAheadLog::~WriteAheadLog() {
  if (log_fd_ != -1) {
    close(log_fd_);
  }
}

absl::StatusOr<std::unique_ptr<WriteAheadLog>> WriteAheadLog::CreateLog(
    const std::string& log_file_path) {
  int log_fd = open(log_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                    S_IRUSR | S_IWUSR);
  if (log_fd < 1) {
    return absl::ErrnoToStatus(errno, "open failed. error obtaing log fd.");
  }
  return std::unique_ptr<WriteAheadLog>(new WriteAheadLog(log_fd));
}

absl::Status WriteAheadLog::LogRequest(const Request& request) {
  if (log_fd_ < 0) {
    return absl::InternalError("Log file descriptor is invalid");
  }

  ABSL_RETURN_IF_ERROR(ValidateWriteRequest(request));

  std::string request_log = ConvertRequestToString(request);
  size_t bytes_written =
      write(log_fd_, request_log.c_str(), request_log.size());
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
  if (log_fd_ == -1){
    return absl::OkStatus();
  }
  absl::Status status = absl::OkStatus();
  if (close(log_fd_) < 0){
    status = absl::ErrnoToStatus(errno, "fclose failed.");
  }
  log_fd_ = -1;
  return status;
}

std::string WriteAheadLog::ConvertRequestToString(const Request& request) {
  std::string log = absl::StrCat("PUT:", request.key, ":", *request.value);
  return log;
}
}  // namespace kvstore