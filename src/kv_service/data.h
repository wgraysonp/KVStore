#ifndef SRC_SERVICE_KVSTORE_H_
#define SRC_SERVICE_KVSTORE_H_

#include <string>
#include <optional>

namespace kvstore { 

  enum class RequestType {
    GET = 1,
    PUT = 2,
  };

  struct Request {
    RequestType request_type;
    std::string key;
    std::optional<std::string> value = std::nullopt;
  };

} // namespace kv_store


#endif // SRC_SERVICE_KVSTORE_H_