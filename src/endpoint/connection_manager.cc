#include "src/endpoint/connection_manager.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "absl/cleanup/cleanup.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "src/endpoint/client_connection.h"
#include "src/endpoint/socket_utils.h"

namespace endpoint {

ConnectionManager::~ConnectionManager() { Stop(); }

absl::Status ConnectionManager::BindAndListen() {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return absl::ErrnoToStatus(errno, "Failed to open server socket");
  }

  int opt = 1;
  if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to set SO_REUSEADDR");
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port_);

  if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    return absl::ErrnoToStatus(
        errno, absl::StrFormat("Failed to bind to port %d", port_));
  }

  if (listen(listen_fd_, 128) < 0) {
    return absl::ErrnoToStatus(errno, "Listen command failed");
  }

  ABSL_RETURN_IF_ERROR(SetNonBlocking(listen_fd_));

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    return absl::ErrnoToStatus(errno, "Failed to create epoll instance");
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to register listen_fd to epoll");
  }

  is_running_ = true;
  LOG(INFO) << absl::StrFormat("Succesfully listening on port %d ...", port_);
  return absl::OkStatus();
}

absl::Status ConnectionManager::RunLoop() {
  auto cleanup = absl::MakeCleanup([this]() {
    if (listen_fd_ >= 0) {
      close(listen_fd_);
      listen_fd_ = -1;
    }
    if (epoll_fd_ >= 0) {
      close(epoll_fd_);
      epoll_fd_ = -1;
    }
  });

  std::vector<epoll_event> events;
  events.reserve(max_events_);

  while (is_running_) {
    events.resize(max_events_);
    // set timeout to 100 ms to periodically check the is_running_ flag
    int num_fds = epoll_wait(epoll_fd_, events.data(), max_events_, 100);
    if (num_fds < 0) {
      // if the wait loop is interupted by an OS signal its not fatal
      if (errno == EINTR) continue;
      return absl::ErrnoToStatus(errno, "Epoll failed");
    }
    events.resize(num_fds);
    ProcessActiveEpollEvents(events);
  }
  return absl::OkStatus();
}

void ConnectionManager::Stop() { is_running_ = false; }

void ConnectionManager::ProcessActiveEpollEvents(
    const std::vector<epoll_event>& events) {
  for (const auto& event : events) {
    int active_fd = event.data.fd;
    if (active_fd == listen_fd_) {
      if (absl::Status status = AcceptNewConnection(); !status.ok()) {
        LOG(ERROR) << "Failed to accept new client connetion. Error message: "
                   << status.message();
      }
    } else {
      if (absl::Status status = HandleExistingClientEvent(event);
          !status.ok()) {
        LOG(ERROR) << "HandleExistingClientEvent faild. Error message: "
                   << status.message();
      }
    }
  }
}

absl::Status ConnectionManager::AcceptNewConnection() {
  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);
  int client_fd =
      accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
  if (client_fd < 0) {
    return absl::ErrnoToStatus(errno, "Accept failed");
  }

  auto close_fd = absl::MakeCleanup([client_fd]() { close(client_fd); });

  ABSL_RETURN_IF_ERROR(SetNonBlocking(client_fd));

  epoll_event client_ev{};
  client_ev.events = EPOLLIN | EPOLLET;
  client_ev.data.fd = client_fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrFormat("Failed to track client fd %d in epoll tree",
                               client_fd));
  }

  active_connections_.emplace(client_fd, client_fd, rpc_registry_);

  std::move(close_fd).Cancel();
  return absl::OkStatus();
}

absl::Status ConnectionManager::HandleExistingClientEvent(
    const epoll_event& ev) {
  const int client_fd = ev.data.fd;

  auto it = active_connections_.find(client_fd);
  if (it == active_connections_.end()) {
    LOG(WARNING) << "Client fd " << client_fd
                 << " not found in active connections map";
    return absl::OkStatus();
  }
  ClientConnection& connection = it->second;

  absl::StatusOr<ConnectionStatus> connection_status = connection.HandleEvent();

  if (!connection_status.ok()) {
    LOG(ERROR) << "Handle Event failed with error message: "
               << connection_status.status().ToString()
               << " Closing connection for client fd " << client_fd;
    active_connections_.erase(client_fd);
  }

  switch (connection_status.value()) {
    case ConnectionStatus::DISCONNECTED_ERROR:
      LOG(INFO) << "Client " << client_fd
                << " disconnected with error. Closing connection.";
      active_connections_.erase(client_fd);
      break;
    case ConnectionStatus::DISCONNECTED_SUCCESS:
      LOG(INFO) << "Client " << client_fd
                << " disconnected successfully. Closing connection.";
      active_connections_.erase(client_fd);
      break;
    case ConnectionStatus::CONNECTED_READ:
    case ConnectionStatus::CONNECTED_DISPATCH:
    case ConnectionStatus::CONNECTED_WRITE:
      LOG(INFO) << "Event for client " << client_fd
                << "handled successfully. Returning to epoll loop.";
      break;
    default:
      active_connections_.erase(client_fd);
      return absl::InternalError(absl::StrFormat(
          "Unknown connection status when handling event for client %d. "
          "Closing connection. This should never happend.",
          client_fd));
  }

  return absl::OkStatus();

}  // namespace endpoint