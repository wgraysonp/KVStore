#include "src/storage/key_value_store.h"

#include <memory>
#include <mutex>
#include <shared_mutex>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "src/kv_service/data.h"
#include "src/storage/write_ahead.h"

namespace kvstore {

absl::StatusOr<std::unique_ptr<KVStore>> KVStore::CreateKVStore(
    std::unique_ptr<IWriteAheadLog> log) {

  ABSL_ASSIGN_OR_RETURN((absl::flat_hash_map<std::string, std::string> store),
                        log->RecoverKVStore());

  return std::unique_ptr<KVStore>(
      new KVStore(std::move(store), std::move(log)));
}

Response KVStore::ProcessRequest(const Request& request){
  if (request.request_type == RequestType::GET ){
    return Get(request);
  } else if (request.request_type == RequestType::PUT) {
    return Put(request);
  } else {
    Response response {
      .response_status = ResponseStatus::InvalidRequest,
    };
    return response;
  }
}

KVStore::KVStore(absl::flat_hash_map<std::string, std::string> store,
                 std::unique_ptr<IWriteAheadLog> log)
    : store_(std::move(store)), write_ahead_log_(std::move(log)) {};

Response KVStore::Get(const Request& request) {
  std::shared_lock<std::shared_mutex> log(kv_mutex_);

  Response response{};

  if (auto it = store_.find(request.key); it != store_.end()) {
    response.response_status = ResponseStatus::Ok;
    response.value = it->second;
  } else {
    response.response_status = ResponseStatus::KeyNotFoundError;
  }
  return response;
}

Response KVStore::Put(const Request& request) {
  std::unique_lock<std::shared_mutex> lock(kv_mutex_);

  Response response{};

  if (absl::Status status = write_ahead_log_->LogRequest(request);
      !status.ok()) {
    response.response_status = ResponseStatus::InternalError;
  }

  auto [unused_it, inserted] =
      store_.insert_or_assign(request.key, *request.value);

  if (inserted) {
    LOG(INFO) << "Adding new key " << request.key << " with value "
              << *request.value;
  } else {
    LOG(INFO) << "Value at key " << request.key << " updated to "
              << *request.value;
  }

  response.response_status = ResponseStatus::Ok;
  return response;
}
}  // namespace kvstore
