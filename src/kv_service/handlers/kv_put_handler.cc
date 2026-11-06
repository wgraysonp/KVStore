#include "src/kv_service/handlers/kv_put_handler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "src/common/consts.h"
#include "src/common/utils/socket_utils.h"

namespace kvstore {
namespace {
absl::StatusOr<std::size_t> ConsumeAndReadSizeHeader(
    absl::string_view& mutable_payload) {
  if (mutable_payload.size() < common::HEADER_INT_SIZE) {
    return absl::InvalidArgumentError("payload does not contain size of key");
  }

  ABSL_ASSIGN_OR_RETURN(uint32_t key_size,
                        common::ReadUint32(mutable_payload.substr(
                            0, common::HEADER_INT_SIZE)));
  mutable_payload.remove_prefix(
      std::min(common::HEADER_INT_SIZE, mutable_payload.size()));

  return static_cast<std::size_t>(key_size);
}
}  // namespace

absl::Status KVPutRequest::DeSerialize() {
  absl::string_view payload = raw_message_.request_payload;

  ABSL_ASSIGN_OR_RETURN(std::size_t key_size,
                        ConsumeAndReadSizeHeader(payload));

  if (payload.size() < key_size) {
    return absl::InvalidArgumentError(
        "fewer payload key bytes than indicated by size header");
  }

  key_ = payload.substr(0, std::min(key_size, payload.size()));
  payload.remove_prefix(std::min(key_size, payload.size()));

  ABSL_ASSIGN_OR_RETURN(std::size_t val_size,
                        ConsumeAndReadSizeHeader(payload));

  if (payload.size() < val_size) {
    return absl::InvalidArgumentError(
        "fewer payload value bytes than indicated by size header");
  }

  value_ = payload.substr(0, val_size);

  return absl::OkStatus();
}

absl::StatusOr<std::string> KVPutResponse::Serialize() {
  return std::move(value_);
}

absl::Status KVPutHandler::ValidateRequest() {
  absl::string_view key = request_.get_key();
  if (key.size() == 0) {
    return absl::InvalidArgumentError("Request key is empty.");
  }
  if (key.size() > common::MAX_KEY_SIZE) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Requested key is too large. Max key size: %s", common::MAX_KEY_SIZE));
  }

  absl::string_view value = request_.get_value();
  if (value.size() == 0) {
    return absl::InvalidArgumentError("Request value is empty");
  }
  return absl::OkStatus();
}

absl::Status KVPutHandler::RunSync() {
  ABSL_RETURN_IF_ERROR(ValidateRequest());

  ABSL_RETURN_IF_ERROR(
      kv_store_->Put(request_.get_key(), request_.get_value()));

  return absl::OkStatus();
}
}  // namespace kvstore