#include "src/storage/key_value_store.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "src/storage/write_ahead.h"

namespace kvstore {

absl::StatusOr<std::shared_ptr<KVStore>> KVStore::CreateKVStore(
    std::unique_ptr<IWriteAheadLog> log) {
  ABSL_ASSIGN_OR_RETURN((absl::flat_hash_map<std::string, std::string> store),
                        log->RecoverKVStore());

  return std::shared_ptr<KVStore>(
      new KVStore(std::move(store), std::move(log)));
}

KVStore::KVStore(absl::flat_hash_map<std::string, std::string> store,
                 std::unique_ptr<IWriteAheadLog> log)
    : store_(std::move(store)), write_ahead_log_(std::move(log)) {};

absl::StatusOr<std::string> KVStore::Get(absl::string_view key) {
  std::shared_lock<std::shared_mutex> log(kv_mutex_);

  if (auto it = store_.find(key); it != store_.end()) {
    return it->second;
  }
  return absl::NotFoundError(absl::StrFormat("Key %s not found", key));
}

absl::Status KVStore::Put(absl::string_view key, absl::string_view value) {
  std::unique_lock<std::shared_mutex> lock(kv_mutex_);

  if (absl::Status status = write_ahead_log_->LogWriteRequest(key, value);
      !status.ok()) {
    return absl::InternalError("Failed to log write request");
  }

  auto [unused_it, inserted] = store_.insert_or_assign(key, value);

  if (inserted) {
    LOG(INFO) << "Adding new key " << key << " with value " << value;
  } else {
    LOG(INFO) << "Value at key " << key << " updated to " << value;
  }

  return absl::OkStatus();
}
}  // namespace kvstore
