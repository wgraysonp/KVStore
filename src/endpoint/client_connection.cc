#include "src/endpoint/client_connection.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "src/endpoint/consts.h"
#include "src/endpoint/socket_utils.h"
#include "src/rpc_framework/data.h"

namespace endpoint {

absl::Status ClientConnection::MakeNonBlocking() {
  ABSL_RETURN_IF_ERROR(SetNonBlocking(client_fd_));
  return absl::OkStatus();
}

absl::StatusOr<ConnectionStatus> ClientConnection::HandleEvent() {
  while (true) {
    if (connection_status_ == ConnectionStatus::CONNECTED_READ) {
      ABSL_ASSIGN_OR_RETURN(ReadState read_state, PerformClientRead());
      switch (read_state) {
        case ReadState::kSUCCESS:
          connection_status_ = ConnectionStatus::CONNECTED_DISPATCH;
          break;
        case ReadState::kEAGAIN:
          // the read did not finish. Return the persistant read state to caller
          return connection_status_;
        case ReadState::kEOF:
          connection_status_ = ConnectionStatus::DISCONNECTED_SUCCESS;
          return connection_status_;
      }
    } else if (connection_status_ == ConnectionStatus::CONNECTED_DISPATCH) {
      rpc::RawMessage msg = MoveClientReadDataToRawMessage();
      ABSL_ASSIGN_OR_RETURN(response_buffer_,
                            rpc_registry_->DispatchRequest(std::move(msg)));
      connection_status_ = ConnectionStatus::CONNECTED_WRITE;
    } else if (connection_status_ == ConnectionStatus::CONNECTED_WRITE) {
      ABSL_ASSIGN_OR_RETURN(WriteState write_state, PerformClientWrite());

      if (write_state == WriteState::kEAGAIN) {
        return connection_status_;
      }

      // reset to prepare for future events
      response_buffer_.clear();
      response_bytes_written_ = 0;
      connection_status_ = ConnectionStatus::CONNECTED_READ;
      break;
    }
  }
  return connection_status_;
}

absl::StatusOr<ReadState> ClientConnection::PerformClientRead() {
  if (size_header_bytes_read_ < HEADER_INT_SIZE) {
    ABSL_ASSIGN_OR_RETURN(
        ReadResult read_result,
        ReadFromClient(size_header_buffer_, size_header_bytes_read_,
                       HEADER_INT_SIZE - size_header_bytes_read_, client_fd_));

    size_header_bytes_read_ += read_result.bytes_read;

    // read did not complete due to EAGAIN or EWOULDBLOCK.
    // yeild and wait for next event.
    if (read_result.state != ReadState::kSUCCESS) {
      return read_result.state;
    }
  }

  if (rpc_id_header_bytes_read_ < HEADER_INT_SIZE) {
    ABSL_ASSIGN_OR_RETURN(
        ReadResult read_result,
        ReadFromClient(rpc_id_buffer_, rpc_id_header_bytes_read_,
                       HEADER_INT_SIZE - rpc_id_header_bytes_read_,
                       client_fd_));

    rpc_id_header_bytes_read_ += read_result.bytes_read;
    if (read_result.state == ReadState::kEOF) {
      return absl::CancelledError("Client connection closed mid read");
    } else if (read_result.state == ReadState::kEAGAIN) {
      return read_result.state;
    }
  }

  // TODO: clean this up and maybe move it to a helper.

  std::size_t expected_payload_size =
      static_cast<std::size_t>(ReadUint32(size_header_buffer_));

  if (expected_payload_size > MAX_CLIENT_PAYLOAD_SIZE) {
    return absl::InvalidArgumentError("Requested client payload is too large");
  } else if (expected_payload_size == 0) {
    LOG(INFO) << "Client requested payload size is zero. Skipping payload read";
    return ReadState::kSUCCESS;
  }

  if (payload_bytes_read_ < expected_payload_size) {
    ABSL_ASSIGN_OR_RETURN(
        ReadResult read_result,
        ReadFromClient(payload_buffer_, payload_bytes_read_,
                       expected_payload_size - payload_bytes_read_,
                       client_fd_));

    payload_bytes_read_ += read_result.bytes_read;
    if (read_result.state == ReadState::kEOF) {
      return absl::CancelledError("Client connection closed mid read");
    }
    return read_result.state;
  }

  return absl::InternalError(absl::StrFormat(
      "PerformClientRead() skipped payload read for unexpected reason. "
      "payload_bytes_read_: %d, expected_payload_size %d",
      payload_bytes_read_, expected_payload_size));
}

absl::StatusOr<WriteState> ClientConnection::PerformClientWrite() {
  std::size_t total_response_bytes = response_buffer_.size();
  if (response_bytes_written_ < total_response_bytes) {
    ABSL_ASSIGN_OR_RETURN(
        WriteResult write_result,
        WriteToClient(response_buffer_, response_bytes_written_,
                      total_response_bytes - response_bytes_written_,
                      client_fd_));
    response_bytes_written_ += write_result.bytes_written;
    return write_result.state;
  }

  return absl::InternalError(absl::StrFormat(
      "client write not performed for unexpected reason. At time of call - "
      "total_response_bytes: %d, response_bytes_written: %d",
      total_response_bytes, response_bytes_written_));
}

rpc::RawMessage ClientConnection::MoveClientReadDataToRawMessage() {
  rpc::RawMessage msg;

  msg.payload_size = ReadUint32(size_header_buffer_);
  size_header_buffer_.clear();
  size_header_bytes_read_ = 0;

  msg.rpc_id = ReadUint32(rpc_id_buffer_);
  rpc_id_buffer_.clear();
  rpc_id_header_bytes_read_ = 0;

  msg.request_payload = std::move(payload_buffer_);
  payload_buffer_.clear();
  payload_bytes_read_ = 0;

  return msg;
}

}  // namespace endpoint