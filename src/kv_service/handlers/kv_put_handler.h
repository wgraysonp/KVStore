#ifndef KV_SERVICE_HANDLERS_KV_PUT_HANDLER_H_
#define KV_SERVICE_HANDLERS_KV_PUT_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "src/rpc_framework/data.h"
#include "src/rpc_framework/rpc_interface.h"
#include "src/storage/key_value_store.h"

namespace kvstore {

class KVPutRequest : public rpc::Request {
 public:
  KVPutRequest(rpc::RawMessage&& raw_message)
      : rpc::Request(std::move(raw_message)) {};
  absl::Status DeSerialize() override;
  absl::string_view get_key() { return key_; }
  absl::string_view get_value() { return value_; }

 private:
  absl::string_view key_;
  absl::string_view value_;
  // rpc::RawMessage raw_message_;
};

class KVPutResponse : public rpc::Response {
 public:
  absl::StatusOr<std::string> Serialize() override;

  const std::string& get_value() { return value_; }
  std::string* mutable_value() { return &value_; }

 private:
  std::string value_ = "OK";
};

class KVPutHandler : public rpc::Handler<KVPutRequest, KVPutResponse> {
 public:
  KVPutHandler(rpc::RawMessage&& raw_message, std::shared_ptr<KVStore> kv_store)
      : rpc::Handler<KVPutRequest, KVPutResponse>(std::move(raw_message)),
        kv_store_(std::move(kv_store)) {};

 protected:
  absl::Status RunSync() override;

 private:
  absl::Status ValidateRequest();
  std::shared_ptr<KVStore> kv_store_;
};

class KVPutHandlerFactory : public rpc::IHandlerFactory {
 public:
  KVPutHandlerFactory(std::shared_ptr<KVStore> kv_store)
      : kv_store_(std::move(kv_store)) {};
  std::unique_ptr<rpc::IHandler> CreateHandler(
      rpc::RawMessage&& raw_message) override {
    return std::make_unique<KVPutHandler>(std::move(raw_message), kv_store_);
  }

 private:
  std::shared_ptr<KVStore> kv_store_;
};

}  // namespace kvstore

#endif  // KV_SERVICE_HANDLERS_KV_PUT_HANDLER_H_