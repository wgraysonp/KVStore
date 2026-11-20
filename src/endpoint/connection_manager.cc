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
#include "absl/time/time.h"
#include "src/common/utils/socket_utils.h"
#include "src/endpoint/client_connection.h"

namespace endpoint {

ConnectionManager::~ConnectionManager() {
  Stop();
  if (listen_fd_ >= 0) {
    close(listen_fd_);
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
  }
}

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

  ABSL_RETURN_IF_ERROR(common::SetNonBlocking(listen_fd_));

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

absl::Status ConnectionManager::LoopForever() {
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

  absl::Time last_timeout_check = absl::Now();

  while (is_running_) {
    ABSL_RETURN_IF_ERROR(LoopOnce(100));
    if (absl::Now() - last_timeout_check >= timeout_check_frequency_) {
      CloseExpiredConnections(absl::Now());
      last_timeout_check = absl::Now();
    }
  }
  return absl::OkStatus();
}

absl::Status ConnectionManager::LoopOnce(int time_out) {
  events_.resize(max_events_);
  // set timeout to 100 ms to periodically check the is_running_ flag
  int num_fds = epoll_wait(epoll_fd_, events_.data(), max_events_, time_out);
  if (num_fds < 0) {
    // if the wait loop is interupted by an OS signal its not fatal
    if (errno == EINTR) {
      return absl::OkStatus();
    }
    return absl::ErrnoToStatus(errno, "Epoll failed");
  }
  events_.resize(num_fds);
  ProcessActiveEpollEvents(events_);

  return absl::OkStatus();
}

void ConnectionManager::CloseExpiredConnections(absl::Time threshold_time) {
  std::vector<int> expired_fds =
      timer_list_->ClearExpiredConnections(threshold_time);
  for (const auto fd : expired_fds) {
    if (const auto it = active_connections_.find(fd);
        it == active_connections_.end()) {
      LOG(ERROR) << "Close expired connections attempted to close a "
                    "non-existant connection";
    } else {
      it->second->TimeOut();
    }
  }
}

void ConnectionManager::Stop() { is_running_ = false; }

void ConnectionManager::ProcessActiveEpollEvents(
    const std::vector<epoll_event>& events) {
  for (const auto& event : events) {
    if (event.data.fd == listen_fd_) {
      if (absl::Status status = AcceptNewConnection(); !status.ok()) {
        LOG(ERROR) << "Failed to accept new client connetion. Error message: "
                   << status.message();
      }
    } else {
      HandleExistingClientEvent(event);
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

  epoll_event client_ev{};
  client_ev.events = EPOLLIN | EPOLLET;
  client_ev.data.fd = client_fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrFormat("Failed to track client fd %d in epoll tree",
                               client_fd));
  }

  DisconnectCallback on_disconnect = [weak_manager = weak_from_this(),
                                      client_fd]() {
    if (auto manager = weak_manager.lock()) {
      manager->timer_list_->DeleteConnection(client_fd);
      manager->active_connections_.erase(client_fd);
    }
  };

  auto connection = connection_factory_->CreateClientConnection(
      client_fd, rpc_registry_, std::move(on_disconnect));

  ABSL_RETURN_IF_ERROR(timer_list_->AddNewConnection(client_fd, absl::Now()));
  active_connections_.emplace(client_fd, connection);

  std::move(close_fd).Cancel();

  // if MakeNonBlocking fails, the callback on_disconnect will be executed.
  // Because of this, be sure to call this after adding the connection to
  // the active connections map and timer list
  connection->MakeNonBlocking();
  return absl::OkStatus();
}

void ConnectionManager::HandleExistingClientEvent(
    const epoll_event& ev) {
  const int client_fd = ev.data.fd;

  if (absl::Status timer_refresh_status =
          timer_list_->RefreshConnection(client_fd, absl::Now());
      !timer_refresh_status.ok()) {
    LOG(WARNING) << "Failed to refresh connection timer for client "
                 << client_fd
                 << ". Error message: " << timer_refresh_status.ToString();
  }

  const auto it = active_connections_.find(client_fd);
  if (it == active_connections_.end()) {
    LOG(WARNING) << "Client fd " << client_fd
                 << " not found in active connections map";
    return;
  }
  it->second->HandleEvent();
}
}  // namespace endpoint