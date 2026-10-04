#ifndef SRC_SERVICE_KVSTORE_H_
#define SRC_SERVICE_KVSTORE_H_

#include <optional>
#include <string>

namespace kvstore {

enum class RequestType {
  GET = 1,
  PUT = 2,
};

enum class ResponseStatus {
  Ok = 1,
  KeyNotFoundError = 2,
  InternalError = 3,
  InvalidRequest = 4,
};

struct Request {
  RequestType request_type;
  std::string key;
  std::optional<std::string> value = std::nullopt;

  auto operator<=>(const Request&) const = default;
};

struct Response {
  ResponseStatus response_status;
  std::optional<std::string> value = std::nullopt;

  auto operator<=>(const Response&) const = default;
};

}  // namespace kvstore

#endif  // SRC_SERVICE_KVSTORE_H_