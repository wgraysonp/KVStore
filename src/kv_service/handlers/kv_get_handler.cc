#include "src/kv_service/handlers/kv_get_handler.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

namespace kvstore {

absl::Status KVGetRequest::DeSerialize(rpc::RawMessage raw_request) {
  key_ = std::move(raw_request.request_payload);
  return absl::OkStatus();
}

absl::StatusOr<std::string> KVGetResponse::Serialize() {
  return std::move(value_);
}

absl::Status KVGetHandler::ValidateRequest() {
  const std::string& key = request_.get_key();
  if (key.size() == 0) {
    return absl::InvalidArgumentError("Request key is empty.");
  }
  if (key.size() > MAX_KEY_SIZE) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Requested key is too large. Max key size: %s", MAX_KEY_SIZE));
  }
  return absl::OkStatus();
}

absl::Status KVGetHandler::RunSync() {
  ABSL_RETURN_IF_ERROR(ValidateRequest());

  ABSL_ASSIGN_OR_RETURN(std::string value, kv_store_->Get(request_.get_key()));
  *response_.mutable_value() = std::move(value);
  return absl::OkStatus();
}
}  // namespace kvstore