#include "src/storage/write_ahead.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
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

WriteAheadLog::WriteAheadLog(int log_fd, std::string log_file_path)
    : log_fd_(log_fd), log_file_path_(std::move(log_file_path)) {};

WriteAheadLog::~WriteAheadLog() {
  if (log_fd_ != -1) {
    close(log_fd_);
  }
}

absl::StatusOr<std::unique_ptr<WriteAheadLog>> WriteAheadLog::CreateLog(
    std::string log_file_path) {
  int log_fd = open(log_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                    S_IRUSR | S_IWUSR);
  if (log_fd < 1) {
    return absl::ErrnoToStatus(errno, "open failed. error obtaing log fd.");
  }
  return std::unique_ptr<WriteAheadLog>(
      new WriteAheadLog(log_fd, std::move(log_file_path)));
}

absl::Status WriteAheadLog::LogRequest(const Request& request) {
  if (log_fd_ < 0) {
    return absl::InternalError("Log file descriptor is invalid");
  }

  ABSL_RETURN_IF_ERROR(ValidateWriteRequest(request));

  const std::string request_log = ConvertRequestToString(request);
  const size_t bytes_written =
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

absl::StatusOr<absl::flat_hash_map<std::string, std::string>>
WriteAheadLog::RecoverKVStore() {
  int fd = open(log_file_path_.c_str(), O_RDONLY);
  if (fd < 0) {
    return absl::ErrnoToStatus(errno, "Failed to open log for recovery.");
  }

  absl::flat_hash_map<std::string, std::string> key_value_map;

  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    close(fd);
    return absl::ErrnoToStatus(errno, "fstat failed to get log file stats.");
  }
  if (sb.st_size == 0) {
    LOG(INFO) << "Log is empty. Log recovery returning empty hashmap";
    return key_value_map;
  }

  char* file_buffer = static_cast<char*>(
      mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (file_buffer == MAP_FAILED) {
    close(fd);
    return absl::ErrnoToStatus(errno,
                               "mmap failed to map log file to address space.");
  }

  // let the OS know we are going to read the entire file sequentially
  // to optimize the loading process
  madvise(file_buffer, sb.st_size, MADV_SEQUENTIAL);

  const absl::string_view log_data(file_buffer, sb.st_size);

  // if the last line doesnt end in a newline character
  // it is invalid due to a possible crash during write
  const bool last_line_valid = log_data.back() == '\n';

  const std::vector<absl::string_view> lines =
      absl::StrSplit(log_data, '\n', absl::SkipEmpty());

  int processed = 0;

  for (const absl::string_view view : lines) {
        // dont read the last line if it was corrupted
    // TODO: If this is found, the line needs to be deleted from the log
    if (processed == lines.size() - 1 && !last_line_valid) break;

    // if the line doesnt end in a newline character it is invalid.
    // this is mainly the case if there is a crash during a write call
    const std::pair<absl::string_view, absl::string_view> kv =
        absl::StrSplit(view, ':');
    key_value_map[kv.first] = kv.second;

    processed++;
  }

  munmap(file_buffer, sb.st_size);
  close(fd);

  return key_value_map;
}

absl::Status WriteAheadLog::Close() {
  if (log_fd_ == -1) {
    return absl::OkStatus();
  }
  absl::Status status = absl::OkStatus();
  if (close(log_fd_) < 0) {
    status = absl::ErrnoToStatus(errno, "fclose failed.");
  }
  log_fd_ = -1;
  return status;
}

std::string WriteAheadLog::ConvertRequestToString(const Request& request) {
  std::string log = absl::StrCat(request.key, ":", *request.value);
  log += '\n';
  return log;
}
}  // namespace kvstore