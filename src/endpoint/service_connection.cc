#include "src/endpoint/service_connection.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "src/common/consts.h"
#include "src/common/utils/socket_utils.h"

namespace endpoint {
namespace {
absl::StatusOr<common::WriteState> WriteSingleStage(
    absl::string_view payload, std::size_t& prev_bytes_written, int fd) {
  std::size_t total_bytes = payload.size();

  ABSL_ASSIGN_OR_RETURN(
      common::WriteResult write_result,
      common::WriteToClient(payload, prev_bytes_written,
                            total_bytes - prev_bytes_written, fd));

  prev_bytes_written += write_result.bytes_written;

  return write_result.state;
}
}  // namespace

absl::Status ServiceConnection::Receive() {
  read_buffer_.resize(common::READ_BUFFER_SIZE);
  ABSL_ASSIGN_OR_RETURN(common::ReadResult read_result,
                        common::ReadFromClient(
                            read_buffer_, 0, read_buffer_.size(), service_fd_));
  if (read_result.state == common::ReadState::kEOF) {
    LOG(INFO) << "Dropped connectiom with peer service at fd: " << service_fd_;
    return absl::OkStatus();
  }

  // PARSE BUFFER HERE

  return absl::OkStatus();
}

absl::Status ServiceConnection::SendQueuedPackages() {
  while (!outbound_package_queue_.empty()) {
    ABSL_ASSIGN_OR_RETURN(common::WriteState write_state,
                          WriteSinglePackage(outbound_package_queue_.front()));

    switch (write_state) {
      case common::WriteState::kEAGAIN:
        return absl::OkStatus();
      case common::WriteState::kSUCCESS:
        outbound_package_queue_.pop();
        break;
      default:
        return absl::InternalError(
            absl::StrFormat("Unhandle enum value %d encountered in while "
                            "sending queued packages",
                            static_cast<int>(write_state)));
    }
  }
  return absl::OkStatus();
}

absl::Status ServiceConnection::QueueOutboundRequest(
    uint32_t rpc_id, uint32_t request_id, std::string request_bytes,
    absl::AnyInvocable<void(std::string)> request_callback) {
  auto [unused_it, inserted] = request_id_to_callback_map_.try_emplace(
      request_id, std::move(request_callback));
  if (!inserted) {
    return absl::AlreadyExistsError(absl::StrFormat(
        "A request callback already exists for id %d", request_id));
  }

  std::string header_bytes = common::ConvertUint32ToNetworkBytes(
      static_cast<uint32_t>(request_bytes.size()));
  std::string rpc_id_bytes = common::ConvertUint32ToNetworkBytes(rpc_id);
  std::string request_id_bytes =
      common::ConvertUint32ToNetworkBytes(request_id);

  outbound_package_queue_.emplace(
      std::move(header_bytes), std::move(rpc_id_bytes),
      std::move(request_id_bytes), std::move(request_bytes));
  return absl::OkStatus();
}

absl::StatusOr<common::WriteState> ServiceConnection::WriteSinglePackage(
    ServiceIOPackage& mutable_outbound_package) {
  switch (mutable_outbound_package.stage) {
    case ServiceIOStage::SIZE_HEADER: {
      ABSL_ASSIGN_OR_RETURN(
          common::WriteState write_state,
          WriteSingleStage(mutable_outbound_package.network_order_size_header,
                           mutable_outbound_package.size_header_bytes_processed,
                           service_fd_));
      if (write_state == common::WriteState::kEAGAIN) {
        return write_state;
      }
      mutable_outbound_package.stage = ServiceIOStage::RPC_ID;
    }
      [[fallthrough]];

    case ServiceIOStage::RPC_ID: {
      ABSL_ASSIGN_OR_RETURN(
          common::WriteState write_state,
          WriteSingleStage(mutable_outbound_package.network_order_rpc_id,
                           mutable_outbound_package.rpc_id_bytes_processed,
                           service_fd_));
      if (write_state == common::WriteState::kEAGAIN) {
        return write_state;
      }
      mutable_outbound_package.stage = ServiceIOStage::REQUEST_ID;
    }
      [[fallthrough]];

    case ServiceIOStage::REQUEST_ID: {
      ABSL_ASSIGN_OR_RETURN(
          common::WriteState write_state,
          WriteSingleStage(mutable_outbound_package.network_order_request_id,
                           mutable_outbound_package.request_id_bytes_processed,
                           service_fd_));
      if (write_state == common::WriteState::kEAGAIN) {
        return write_state;
      }
      mutable_outbound_package.stage = ServiceIOStage::PAYLOAD;
    }
      [[fallthrough]];

    case ServiceIOStage::PAYLOAD: {
      ABSL_ASSIGN_OR_RETURN(
          common::WriteState write_state,
          WriteSingleStage(mutable_outbound_package.payload_bytes,
                           mutable_outbound_package.payload_bytes_processed,
                           service_fd_));
      return write_state;
    }
  }

  return absl::InternalError(
      absl::StrFormat("Unhandled enum value: %d",
                      static_cast<int>(mutable_outbound_package.stage)));
}

}  // namespace endpoint
