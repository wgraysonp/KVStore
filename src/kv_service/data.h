#ifndef SRC_SERVICE_KVSTORE_H_
#define SRC_SERVICE_KVSTORE_H_

#include <string>
#include <optional>

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
  };

  struct Response {
    ResponseStatus response_status;
    std::optional<std::string> value = std::nullopt;
  };

} // namespace kv_store


#endif // SRC_SERVICE_KVSTORE_H_