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

#include "absl/cleanup/cleanup.h"
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

// check if the last line in log is corrupted and remove it if so
absl::StatusOr<size_t> ClearCorruptedEntriesFromLog(
    const int fd, const ssize_t old_file_size) {
  if (old_file_size < 0) {
    return absl::InvalidArgumentError("file size must be non-negative");
  } else if (old_file_size == 0) {
    return 0;
  }

  char last_byte = '\0';
  ssize_t new_file_size = old_file_size;
  ssize_t bytes_read;

  bytes_read = pread(fd, &last_byte, 1, old_file_size - 1);
  if (bytes_read < 0) {
    return absl::ErrnoToStatus(errno, "pread failed to read last byte of log.");
  } else if (bytes_read == 0) {
    return absl::InternalError("pread performed short read.");
  }

  // check if last line is corrupted - signaled by the lack of terminating
  // newline character
  if (bytes_read == 1 && last_byte != '\n') {
    std::string buffer;
    buffer.resize(old_file_size);

    bytes_read = pread(fd, &buffer[0], static_cast<size_t>(old_file_size), 0);
    if (bytes_read < 0) {
      return absl::ErrnoToStatus(errno, "pread failed to read entire log file");
    } else if (bytes_read < old_file_size) {
      return absl::InternalError(
          "pread performed short read during bulk file read");
    }

    size_t last_newline = buffer.rfind('\n');

    // if this is false the entire file is corrupted and the new file size
    // should remain zero
    if (last_newline != std::string::npos) {
      new_file_size = last_newline + 1;
    } else {
      new_file_size = 0;
    }

    // truncate the file to new file size which removes last corrupted line
    // if the
    if (ftruncate(fd, new_file_size) < 0) {
      return absl::ErrnoToStatus(errno, "ftruncate failed");
    }
  }
  return static_cast<size_t>(new_file_size);
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
  int fd = open(log_file_path_.c_str(), O_RDWR);
  if (fd < 0) {
    return absl::ErrnoToStatus(errno, "Failed to open log for recovery.");
  }

  auto close_fd = absl::MakeCleanup([fd]() { close(fd); });

  absl::flat_hash_map<std::string, std::string> key_value_map;

  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    return absl::ErrnoToStatus(errno, "fstat failed to get log file stats.");
  }
  if (sb.st_size == 0) {
    LOG(INFO) << "Log is empty. Log recovery returning empty hashmap";
    return key_value_map;
  }

  ABSL_ASSIGN_OR_RETURN(size_t cleaned_file_size,
                        ClearCorruptedEntriesFromLog(fd, sb.st_size));

  char* file_buffer = static_cast<char*>(
      mmap(nullptr, cleaned_file_size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (file_buffer == MAP_FAILED) {
    return absl::ErrnoToStatus(errno,
                               "mmap failed to map log file to address space.");
  }

  // let the OS know we are going to read the entire file sequentially
  // to optimize the loading process
  madvise(file_buffer, cleaned_file_size, MADV_SEQUENTIAL);

  const absl::string_view log_data(file_buffer, cleaned_file_size);

  const std::vector<absl::string_view> lines =
      absl::StrSplit(log_data, '\n', absl::SkipEmpty());

  for (const absl::string_view view : lines) {
    const std::pair<absl::string_view, absl::string_view> kv =
        absl::StrSplit(view, ':');
    key_value_map[kv.first] = kv.second;
  }

  if (munmap(file_buffer, cleaned_file_size) < 0) {
    return absl::ErrnoToStatus(errno, "munmap failed.");
  }

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