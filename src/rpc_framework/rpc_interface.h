#ifndef SRC_RPC_FRAMEWORK_RPC_INTERFACE_H_
#define SRC_RPC_FRAMEWORK_RPC_INTERFACE_H_

#include <concepts>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "src/rpc_framework/data.h"

namespace rpc {

class Request {
 public:
  Request(RawMessage&& raw_message) : raw_message_(std::move(raw_message)) {};
  virtual ~Request() = default;
  virtual absl::Status DeSerialize() = 0;

 protected:
  const RawMessage raw_message_;
};

class Response {
 public:
  virtual ~Response() = default;
  virtual absl::StatusOr<std::string> Serialize() = 0;
};

class IHandler {
 public:
  virtual ~IHandler() = default;
  virtual absl::StatusOr<std::string> HandleRequest() = 0;

 protected:
  virtual absl::Status RunSync() = 0;
};

template <std::derived_from<Request> RequestType,
          std::derived_from<Response> ResponseType>
class Handler : public IHandler {
 public:
  Handler(RawMessage&& raw_message) : request_(raw_message) {};
  Handler& operator=(const Handler&) = delete;
  Handler(const Handler&) = delete;
  Handler& operator=(Handler&&) = delete;
  Handler(Handler&&) = delete;

  absl::StatusOr<std::string> HandleRequest() override {
    ABSL_RETURN_IF_ERROR(request_.DeSerialize());
    ABSL_RETURN_IF_ERROR(RunSync());
    ABSL_ASSIGN_OR_RETURN(std::string response_bytes, response_.Serialize());
    return response_bytes;
  }

 protected:
  RequestType request_;
  ResponseType response_;
};

class IHandlerFactory {
 public:
  virtual ~IHandlerFactory() = default;
  virtual std::unique_ptr<IHandler> CreateHandler(RawMessage&& raw_message) = 0;
};

}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_RPC_INTERFACE_H_