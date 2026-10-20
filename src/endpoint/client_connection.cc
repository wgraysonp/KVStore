#include "src/endpoint/client_connection.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>
#include <string>

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
  if (client_fd_ == -1) {
    return absl::FailedPreconditionError("Client file descriptor is invalid");
  }
  if (connection_status_ == ConnectionStatus::DISCONNECTED_ERROR ||
      connection_status_ == ConnectionStatus::DISCONNECTED_SUCCESS) {
    return absl::FailedPreconditionError(
        "ClientConnection is in a disconnected state and should be deleted.");
  }

  while (true) {
    if (connection_status_ == ConnectionStatus::CONNECTED_READ) {
      connection_status_ = ProcessReadStage();
      if (connection_status_ != ConnectionStatus::CONNECTED_DISPATCH) {
        return connection_status_;
      }
    } else if (connection_status_ == ConnectionStatus::CONNECTED_DISPATCH) {
      connection_status_ = ProcessDispatchStage();
      if (connection_status_ != ConnectionStatus::CONNECTED_WRITE) {
        return connection_status_;
      }
    } else if (connection_status_ == ConnectionStatus::CONNECTED_WRITE) {
      connection_status_ = ProcessWriteStage();
      break;
    }
  }
  return connection_status_;
}

ConnectionStatus ClientConnection::ProcessReadStage() {
  absl::StatusOr<ReadState> read_status = PerformClientRead();
  if (!read_status.ok()) {
    HandleDisconnectOnError(read_status.status());
    return ConnectionStatus::DISCONNECTED_ERROR;
  }
  ReadState read_state = std::move(read_status).value();

  switch (read_state) {
    case ReadState::kSUCCESS:
      return ConnectionStatus::CONNECTED_DISPATCH;
    case ReadState::kEAGAIN:
      LOG(INFO) << "Read for client fd " << client_fd_
                << " interrupted by EAGAIN";
      return ConnectionStatus::CONNECTED_READ;
    case ReadState::kEOF:
      return ConnectionStatus::DISCONNECTED_SUCCESS;
    default:
      LOG(ERROR) << "State unknown in ProcessReadState. Disconnecting";
      HandleDisconnectOnError(absl::InternalError(""));
      return ConnectionStatus::DISCONNECTED_ERROR;
  }
}

ConnectionStatus ClientConnection::ProcessDispatchStage() {
  absl::StatusOr<rpc::RawMessage> move_client_data_status =
      MoveClientReadDataToRawMessage();
  if (!move_client_data_status.ok()) {
    HandleDisconnectOnError(move_client_data_status.status());
    return ConnectionStatus::DISCONNECTED_ERROR;
  }

  rpc::RawMessage msg = std::move(move_client_data_status).value();

  absl::StatusOr<std::string> dispatch_request_status =
      rpc_registry_->DispatchRequest(std::move(msg));
  if (!dispatch_request_status.ok()) {
    HandleDisconnectOnError(dispatch_request_status.status());
    return ConnectionStatus::DISCONNECTED_ERROR;
  }
  response_buffer_ = std::move(dispatch_request_status).value();

  // store the size of the response in network byte order
  response_header_buffer_ =
      ConvertUint32ToNetworkBytes(response_buffer_.size());

  return ConnectionStatus::CONNECTED_WRITE;
}

ConnectionStatus ClientConnection::ProcessWriteStage() {
  absl::StatusOr<WriteState> write_status = PerformClientWrite();
  if (!write_status.ok()) {
    // pass no arguments here to bypass writing error code to client
    // since we failed in write state
    HandleDisconnectOnError();
    return ConnectionStatus::DISCONNECTED_ERROR;
  }

  WriteState write_state = std::move(write_status).value();

  if (write_state == WriteState::kEAGAIN) {
    LOG(INFO) << "Write for client fd " << client_fd_ << "interupted by EAGAIN";
    return ConnectionStatus::CONNECTED_WRITE;
  }

  // reset to prepare for future events
  Reset();
  return ConnectionStatus::CONNECTED_READ;
}

absl::StatusOr<ReadState> ClientConnection::PerformClientRead() {
  if (size_header_bytes_read_ < HEADER_INT_SIZE) {
    absl::StatusOr<ReadResult> read_status =
        ReadFromClient(size_header_buffer_, size_header_bytes_read_,
                       HEADER_INT_SIZE - size_header_bytes_read_, client_fd_);
    if (!read_status.ok()) {
      LOG(ERROR) << "ReadFromClient failed: "
                 << read_status.status().ToString();

      // Do not return invalid argument incase it is accidently written
      // do client. If ReadFromClient returns invalid argument, it is an
      // internal error at this level.
      if (read_status.status().code() == absl::StatusCode::kInvalidArgument) {
        return absl::InternalError("");
      }
      return read_status.status();
    }
    ReadResult read_result = std::move(read_status).value();
    size_header_bytes_read_ += read_result.bytes_read;

    // read did not complete due to EAGAIN or EWOULDBLOCK.
    // yeild and wait for next event.
    if (read_result.state != ReadState::kSUCCESS) {
      return read_result.state;
    }
  }

  if (rpc_id_header_bytes_read_ < HEADER_INT_SIZE) {
    absl::StatusOr<ReadResult> read_status =
        ReadFromClient(rpc_id_buffer_, rpc_id_header_bytes_read_,
                       HEADER_INT_SIZE - rpc_id_header_bytes_read_, client_fd_);

    if (!read_status.ok()) {
      LOG(ERROR) << "ReadFromClient failed: "
                 << read_status.status().ToString();

      // Do not return invalid argument incase it is accidently written
      // do client. If ReadFromClient returns invalid argument, it is an
      // internal error at this level.
      if (read_status.status().code() == absl::StatusCode::kInvalidArgument) {
        return absl::InternalError("");
      }
      return read_status.status();
    }
    ReadResult read_result = std::move(read_status).value();
    rpc_id_header_bytes_read_ += read_result.bytes_read;

    if (read_result.state == ReadState::kEOF) {
      LOG(ERROR) << "Client connction closed mid read";
      return absl::CancelledError("");
    } else if (read_result.state == ReadState::kEAGAIN) {
      return read_result.state;
    }
  }

  // TODO: clean this up and maybe move it to a helper.
  std::size_t expected_payload_size;
  if (absl::StatusOr<uint32_t> status = ReadUint32(size_header_buffer_);
      status.ok()) {
    expected_payload_size = static_cast<std::size_t>(std::move(status).value());
  } else {
    LOG(ERROR) << "ReadUint32 failed: " << status.status().ToString();
    return status.status();
  }

  if (expected_payload_size > MAX_CLIENT_PAYLOAD_SIZE) {
    LOG(ERROR) << "Client requested payload too large";
    return absl::InvalidArgumentError("Requested payload is too large");
  } else if (expected_payload_size == 0) {
    LOG(INFO) << "Client requested payload size is zero. Skipping payload read";
    return ReadState::kSUCCESS;
  }

  if (payload_bytes_read_ < expected_payload_size) {
    absl::StatusOr<ReadResult> read_status =
        ReadFromClient(payload_buffer_, payload_bytes_read_,
                       expected_payload_size - payload_bytes_read_, client_fd_);
    if (!read_status.ok()) {
      LOG(ERROR) << "Read from client failed: "
                 << read_status.status().ToString();

      // Do not return invalid argument incase it is accidently written
      // do client. If ReadFromClient returns invalid argument, it is an
      // internal error at this level.
      if (read_status.status().code() == absl::StatusCode::kInvalidArgument) {
        return absl::InternalError("");
      }
      return read_status.status();
    }
    ReadResult read_result = std::move(read_status).value();
    payload_bytes_read_ += read_result.bytes_read;

    if (read_result.state == ReadState::kEOF) {
      LOG(ERROR) << "Client connection closed mid read";
      return absl::CancelledError("");
    }
    return read_result.state;
  }

  LOG(ERROR) << "PerformClientRead skipped payload read for unexpected reason.";
  return absl::InternalError("");
}

absl::StatusOr<WriteState> ClientConnection::PerformClientWrite() {
  if (response_header_bytes_written_ < HEADER_INT_SIZE) {
    absl::StatusOr<WriteResult> status = WriteToClient(
        response_header_buffer_, response_header_bytes_written_,
        HEADER_INT_SIZE - response_header_bytes_written_, client_fd_);

    if (!status.ok()) {
      LOG(ERROR) << "WriteToClient failed " << status.status().ToString();

      // dont return InvalidArgument if returned by WriteToClient.
      // InvalidArgument messages are written to client by HandleError
      if (status.status().code() == absl::StatusCode::kInvalidArgument) {
        return absl::InternalError("");
      }
      return status.status();
    }
    WriteResult write_result = std::move(status).value();
    response_header_bytes_written_ += write_result.bytes_written;

    if (write_result.state == WriteState::kEAGAIN) {
      return write_result.state;
    }
  }

  if (response_bytes_written_ < response_buffer_.size()) {
    absl::StatusOr<WriteResult> status = WriteToClient(
        response_buffer_, response_bytes_written_,
        response_buffer_.size() - response_bytes_written_, client_fd_);

    if (!status.ok()) {
      LOG(ERROR) << "WriteToClient failed " << status.status().ToString();

      // dont return InvalidArgument if returned by WriteToClient.
      // InvalidArgument messages are written to client by HandleError
      if (status.status().code() == absl::StatusCode::kInvalidArgument) {
        return absl::InternalError("");
      }
      return status.status();
    }
    WriteResult write_result = std::move(status).value();
    response_bytes_written_ += write_result.bytes_written;

    // no need to check the state here. Regardless, it is handled by caller
    return write_result.state;
  }

  LOG(ERROR) << "Client write not performed for unexpected reason.";
  return absl::InternalError("");
}

absl::StatusOr<rpc::RawMessage>
ClientConnection::MoveClientReadDataToRawMessage() {
  rpc::RawMessage msg;

  if (absl::StatusOr<uint32_t> status = ReadUint32(size_header_buffer_);
      !status.ok()) {
    LOG(ERROR) << "ReadUint32 failed: " << status.status().ToString();
    return absl::InternalError("");
  } else {
    msg.payload_size = std::move(status).value();
    size_header_buffer_.clear();
    size_header_bytes_read_ = 0;
  }

  if (absl::StatusOr<uint32_t> status = ReadUint32(rpc_id_buffer_);
      !status.ok()) {
    LOG(ERROR) << "ReadUint32 failed: " << status.status().ToString();
    return absl::InternalError("");
  } else {
    msg.rpc_id = std::move(status).value();
    rpc_id_buffer_.clear();
    rpc_id_header_bytes_read_ = 0;
  }

  msg.request_payload = std::move(payload_buffer_);
  payload_buffer_.clear();
  payload_bytes_read_ = 0;

  return msg;
}

void ClientConnection::HandleDisconnectOnError(const absl::Status& status) {
  if (!status.ok()) {
    HandleError(status);
  }
  Reset();
  close(client_fd_);
  client_fd_ = -1;
}

void ClientConnection::HandleError(const absl::Status& status) {
  switch (status.code()) {
    case absl::StatusCode::kCancelled:
    case absl::StatusCode::kUnavailable:
      // cannot write a message to client if the connection is broken
      return;
    case absl::StatusCode::kInvalidArgument:
    case absl::StatusCode::kOutOfRange:
      if (absl::Status error_write_status =
              WriteErrorMessageToClient(status.ToString(), client_fd_);
          !error_write_status.ok()) {
        LOG(ERROR) << "WriteErrorMessageToClient failed "
                   << error_write_status.ToString();
      }
      break;
    default:
      if (absl::Status error_write_status =
              WriteErrorMessageToClient("Internal Error", client_fd_);
          !error_write_status.ok()) {
        LOG(ERROR) << "WriteErrorMessageToClient failed "
                   << error_write_status.ToString();
      }
      break;
  }
}

void ClientConnection::Reset() {
  size_header_buffer_.clear();
  size_header_bytes_read_ = 0;
  rpc_id_buffer_.clear();
  rpc_id_header_bytes_read_ = 0;
  payload_buffer_.clear();
  payload_bytes_read_ = 0;
  response_header_buffer_.clear();
  response_header_bytes_written_ = 0;
  response_buffer_.clear();
  response_bytes_written_ = 0;
}

}  // namespace endpoint