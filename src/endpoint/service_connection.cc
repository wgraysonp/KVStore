#include "src/endpoint/service_connection.h"

#include <cstdint>
#include <string>

#include "absl/base/optimization.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
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

std::size_t ReadSingleStageFromBuffer(absl::string_view& mutable_read_buffer,
                                      std::string& mutable_target_buffer,
                                      std::size_t max_bytes) {
  absl::string_view chunk = mutable_read_buffer.substr(0, max_bytes);
  mutable_read_buffer.remove_prefix(chunk.size());
  absl::StrAppend(&mutable_target_buffer, chunk);
  return chunk.size();
}

// TODO: implement this.
std::size_t FillSinglePackageFromReadBuffer(ServiceIOPackage& mutable_package,
                                            absl::string_view buffer) {
  std::size_t total_bytes_processed = 0;

  switch (mutable_package.stage) {
    case ServiceIOStage::SIZE_HEADER: {
      std::size_t bytes_to_process =
          common::HEADER_INT_SIZE - mutable_package.size_header_bytes_processed;

      std::size_t stage_bytes_processed = ReadSingleStageFromBuffer(
          buffer, mutable_package.network_order_size_header, bytes_to_process);

      total_bytes_processed += stage_bytes_processed;
      mutable_package.size_header_bytes_processed += stage_bytes_processed;

      if (stage_bytes_processed < bytes_to_process) {
        return total_bytes_processed;
      }
      mutable_package.stage = ServiceIOStage::RPC_ID;
    }
      [[fallthrough]];

    case ServiceIOStage::RPC_ID: {
      std::size_t bytes_to_process =
          common::HEADER_INT_SIZE - mutable_package.rpc_id_bytes_processed;

      std::size_t stage_bytes_processed = ReadSingleStageFromBuffer(
          buffer, mutable_package.network_order_rpc_id, bytes_to_process);

      total_bytes_processed += stage_bytes_processed;
      mutable_package.rpc_id_bytes_processed += stage_bytes_processed;

      if (stage_bytes_processed < bytes_to_process) {
        return total_bytes_processed;
      }
      mutable_package.stage = ServiceIOStage::REQUEST_ID;
    }
      [[fallthrough]];

    case ServiceIOStage::REQUEST_ID: {
      std::size_t bytes_to_process =
          common::HEADER_INT_SIZE - mutable_package.request_id_bytes_processed;

      std::size_t stage_bytes_processed = ReadSingleStageFromBuffer(
          buffer, mutable_package.network_order_request_id, bytes_to_process);

      total_bytes_processed += stage_bytes_processed;
      mutable_package.request_id_bytes_processed += stage_bytes_processed;

      if (stage_bytes_processed < bytes_to_process) {
        return total_bytes_processed;
      }
      mutable_package.stage = ServiceIOStage::PAYLOAD;
    }
      [[fallthrough]];

    case ServiceIOStage::PAYLOAD: {
      ABSL_CHECK_EQ(mutable_package.network_order_size_header.size(), 4)
          << "ReadUint32 requires a 4 byte buffer";
      std::size_t payloan_len = static_cast<std::size_t>(
          common::ReadUint32(mutable_package.network_order_size_header)
              .value());

      std::size_t bytes_to_process =
          payloan_len - mutable_package.payload_bytes_processed;

      std::size_t stage_bytes_processed = ReadSingleStageFromBuffer(
          buffer, mutable_package.payload_bytes, bytes_to_process);

      total_bytes_processed += stage_bytes_processed;
      mutable_package.payload_bytes_processed += stage_bytes_processed;

      if (stage_bytes_processed < bytes_to_process) {
        return total_bytes_processed;
      }
      mutable_package.stage = ServiceIOStage::COMPLETE;
    }
      [[fallthrough]];
    case ServiceIOStage::COMPLETE:
      return total_bytes_processed;
  }

  ABSL_UNREACHABLE();
}
}  // namespace

absl::Status ServiceConnection::HandleInboundRequests() {
  read_buffer_.resize(common::READ_BUFFER_SIZE);
  ABSL_ASSIGN_OR_RETURN(common::ReadResult read_result,
                        common::ReadFromClient(
                            read_buffer_, 0, read_buffer_.size(), service_fd_));

  if (read_result.state == common::ReadState::kEOF) {
    // maybe close this here? can I somehow keep this alive and reconnect?
    LOG(INFO) << "Dropped connection with peer service at fd: " << service_fd_;
    service_fd_ = -1;
    return absl::OkStatus();
  }

  ClearReadBuffer();

  return absl::OkStatus();
}

absl::Status ServiceConnection::SendOutboundRequests() {
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
    case ServiceIOStage::COMPLETE:
      return common::WriteState::kSUCCESS;
  }

  return absl::InternalError(
      absl::StrFormat("Unhandled enum value: %d",
                      static_cast<int>(mutable_outbound_package.stage)));
}

void ServiceConnection::ClearReadBuffer() {
  std::size_t buff_size = read_buffer_.size();
  std::size_t buff_offset = 0;

  while (buff_offset < buff_size) {
    if (inbound_package_queue_.empty() ||
        inbound_package_queue_.back().stage == ServiceIOStage::COMPLETE) {
      inbound_package_queue_.emplace();
    }

    absl::string_view buffer_view =
        absl::ClippedSubstr(read_buffer_, buff_offset);
    buff_offset += FillSinglePackageFromReadBuffer(
        inbound_package_queue_.back(), buffer_view);
  }

  read_buffer_.clear();
}
}  // namespace endpoint
