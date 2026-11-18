#ifndef SRC_RPC_FRAMEWORK_RPC_INTERFACE_H_
#define SRC_RPC_FRAMEWORK_RPC_INTERFACE_H_

#include <concepts>
#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
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
  virtual absl::Status HandleRequest() = 0;
};

template <std::derived_from<Request> RequestType,
          std::derived_from<Response> ResponseType>
class SyncHandler : public IHandler {
 public:
  SyncHandler(RawMessage&& raw_message,
              absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete)
      : request_(std::move(raw_message)),
        on_complete_(std::move(on_complete)) {};
  SyncHandler& operator=(const SyncHandler&) = delete;
  SyncHandler(const SyncHandler&) = delete;
  SyncHandler& operator=(SyncHandler&&) = delete;
  SyncHandler(SyncHandler&&) = delete;

  absl::Status HandleRequest() override {
    ABSL_RETURN_IF_ERROR(request_.DeSerialize());
    ABSL_RETURN_IF_ERROR(RunSync());
    ABSL_ASSIGN_OR_RETURN(std::string response_bytes, response_.Serialize());
    on_complete_(std::move(response_bytes));
    return absl::OkStatus();
  }

 protected:
  virtual absl::Status RunSync() = 0;

  RequestType request_;
  ResponseType response_;
  absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete_;
};

template <std::derived_from<Request> RequestType,
          std::derived_from<Response> ResponseType>
class AsyncHandler : public IHandler {
 public:
  AsyncHandler(
      RawMessage&& raw_message,
      absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete)
      : request_(std::move(raw_message)),
        on_complete_(std::move(on_complete)) {};
  AsyncHandler& operator=(const AsyncHandler&) = delete;
  AsyncHandler(const AsyncHandler&) = delete;
  AsyncHandler& operator=(AsyncHandler&&) = delete;
  AsyncHandler(AsyncHandler&&) = delete;

  absl::Status HandleRequest() override {
    ABSL_RETURN_IF_ERROR(request_.DeSerialize());
    ABSL_RETURN_IF_ERROR(RunAsync());
    return absl::OkStatus();
  }

  void Finish() {
    if (finished_) return;
    finished_ = true;
    on_complete_(response_.Serialize());
  }

  void FinishWithError(absl::Status error_status){
    if (finished_) return;
    finished_ = true;
    on_complete_(error_status);
  }

 protected:
  virtual absl::Status RunAsync() = 0;

  RequestType request_;
  ResponseType response_;
  absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete_;
  bool finished_ = false;
};

class IHandlerFactory {
 public:
  virtual ~IHandlerFactory() = default;
  virtual std::unique_ptr<IHandler> CreateHandler(
      RawMessage&& raw_message,
      absl::AnyInvocable<void(absl::StatusOr<std::string>)> on_complete) = 0;
};

}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_RPC_INTERFACE_H_