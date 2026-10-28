#ifndef SRC_STOREAGE_KEY_VALUE_STORE_H_
#define SRC_STOREAGE_KEY_VALUE_STORE_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "src/storage/write_ahead.h"

namespace kvstore {

class KVStore {
 public:
  static absl::StatusOr<std::shared_ptr<KVStore>> CreateKVStore(
      std::unique_ptr<IWriteAheadLog> log);
  KVStore(const KVStore&) = delete;
  KVStore& operator=(const KVStore&) = delete;
  KVStore(KVStore&&) = default;
  KVStore& operator=(KVStore&&) = default;

  absl::Status Put(absl::string_view key, absl::string_view value);
  absl::StatusOr<std::string> Get(absl::string_view key);
  const absl::flat_hash_map<std::string, std::string>& GetKVMap() const {
    return store_;
  }
  const IWriteAheadLog& GetWriteAheadLog() const { return *write_ahead_log_; }

 private:
  KVStore(absl::flat_hash_map<std::string, std::string> store,
          std::unique_ptr<IWriteAheadLog> log);

  std::shared_mutex kv_mutex_;
  absl::flat_hash_map<std::string, std::string> store_;
  std::unique_ptr<IWriteAheadLog> write_ahead_log_;
};

}  // namespace kvstore

#endif  // SRC_STOREAGE_KEY_VALUE_STORE_H_