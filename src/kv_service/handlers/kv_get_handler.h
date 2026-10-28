#ifndef KV_SERVICE_HANDLERS_KV_GET_HANDLER_H_
#define KV_SERVICE_HANDLERS_KV_GET_HANDLER_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"
#include "src/storage/key_value_store.h"

#define DEFAULT_VAL_SIZE 20
#define MAX_KEY_SIZE 128

namespace kvstore {

class KVGetRequest : public rpc::Request {
 public:
  absl::Status DeSerialize(rpc::RawMessage raw_request) override;
  const std::string& get_key() { return key_; }

 private:
  std::string key_;
};

class KVGetResponse : public rpc::Response {
 public:
  KVGetResponse() { value_.reserve(DEFAULT_VAL_SIZE); }
  absl::StatusOr<std::string> Serialize() override;

  const std::string& get_value() { return value_; }
  std::string* mutable_value() { return &value_; }

 private:
  std::string value_;
};

class KVGetHandler : public rpc::Handler<KVGetRequest, KVGetResponse> {
 public:
  KVGetHandler(std::shared_ptr<KVStore> kv_store)
      : kv_store_(std::move(kv_store)) {};

 protected:
  absl::Status RunSync() override;

 private:
  absl::Status ValidateRequest();
  std::shared_ptr<KVStore> kv_store_;
};

class KVGetHandlerFactory : public rpc::IHandlerFactory {
 public:
  KVGetHandlerFactory(std::shared_ptr<KVStore> kv_store)
      : kv_store_(std::move(kv_store)) {};
  std::unique_ptr<rpc::IHandler> CreateHandler() override {
    return std::make_unique<KVGetHandler>(kv_store_);
  }

 private:
  std::shared_ptr<KVStore> kv_store_;
};

}  // namespace kvstore

#endif  // KV_SERVICE_HANDLERS_KV_GET_HANDLER_H_