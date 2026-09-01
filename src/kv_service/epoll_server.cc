#include "src/kv_service/epoll_server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "absl/base/internal/strerror.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/str_format.h"

namespace kvstore {
namespace {

absl::StatusOr<std::vector<char>> ReadClientPayload(int payload_len,
                                                    int client_fd) {
  std::vector<char> payload(payload_len);
  size_t total_read = 0;
  while (total_read < payload_len) {
    ssize_t n = recv(client_fd, payload.data() + total_read,
                     payload_len - total_read, 0);
    if (n > 0) {
      total_read += n;
      continue;
    }

    if (n == 0) {
      close(client_fd);
      return absl::InternalError(
          absl::StrFormat("Dropped client fd %d mid stream.\n"));
    }

    // if data isnt here yet, spin for 10 microseconds since we are expecting
    // payload_len bytes
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      usleep(10);
      continue;
    }

    // n < 0 and errno is not EAGAIN or EWOULDBLOCK - fatal system error
    close(client_fd);
    return absl::InternalError(
        absl::StrFormat("Dropped client fd %d mid payload read. errno: %s.\n",
                        absl::base_internal::StrError(errno)));
  }

  return payload;
}
}  // namespace

EpollServer::~EpollServer() { Stop(); }

absl::Status EpollServer::Start() {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to open server socket. errno: %s",
                        absl::base_internal::StrError(errno)));
  }

  int opt = 1;
  if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to set SO_REUSEADDR. errno: %s",
                        absl::base_internal::StrError(errno)));
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr =
      local_only_ ? htonl(INADDR_LOOPBACK) : INADDR_ANY;
  server_addr.sin_port = htons(port_);

  if (bind(listen_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    return absl::InternalError(absl::StrFormat(
        "Failed to bind to port %d. Port may be in use. errno: %s", port_,
        absl::base_internal::StrError(errno)));
  }

  if (listen(listen_fd_, 128) < 0) {
    return absl::InternalError(
        absl::StrFormat("Listen command failed. errno: %s",
                        absl::base_internal::StrError(errno)));
  }

  ABSL_RETURN_IF_ERROR(SetNonBlocking(listen_fd_));

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to create epoll instance. errno: %s",
                        absl::base_internal::StrError(errno)));
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to register listen_fd to epoll. errno: %s",
                        absl::base_internal::StrError(errno)));
  }

  is_running_ = true;
  std::cout << absl::StrFormat("Listening on port %d ... /n", port_);
  return absl::OkStatus();
}

absl::Status EpollServer::RunLoop() {
  std::vector<epoll_event> events(max_events_);

  while (is_running_) {
    int num_fds = epoll_wait(epoll_fd_, events.data(), max_events_, 100);
    if (num_fds < 0) {
      // if the wait loop is interupted by an OS signal its not fatal
      if (errno == EINTR) continue;
      return absl::InternalError(absl::StrFormat(
          "Epoll failed. errno: %s", absl::base_internal::StrError(errno)));
    }

    ProcessActiveEpollEvents(events, num_fds);
  }
  return absl::OkStatus();
}

void EpollServer::Stop() {
  is_running_ = false;
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
}

absl::Status EpollServer::SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return absl::InternalError(
        absl::StrFormat("Failed to read flags for descriptor %d. errno: %s", fd,
                        absl::base_internal::StrError(errno)));
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return absl::InternalError(absl::StrFormat(
        "Failed to write O_NONBLOCK flag for descriptor %d. errno: %s", fd,
        absl::base_internal::StrError(errno)));
  }
  return absl::OkStatus();
}

void EpollServer::ProcessActiveEpollEvents(
    const std::vector<epoll_event>& events, int num_fds) {
  for (int i = 0; i < num_fds; ++i) {
    int active_fd = events[i].data.fd;
    if (active_fd == listen_fd_) {
      if (absl::Status status = AcceptNewConnection(); !status.ok()) {
        LOG(ERROR) << "Failed to accept new client connetion. Error message: "
                   << status.message();
      }
    } else {
      if (absl::Status status = HandleClientRead(active_fd); !status.ok()) {
        LOG(ERROR) << "Client read failed. Error message: " << status.message();
      }
    }
  }
}

uint32_t EpollServer::ReadUint32(const char* buffer) {
  uint32_t val;
  std::memcpy(&val, buffer, 4);
  return ntohl(val);
}

absl::Status EpollServer::AcceptNewConnection() {
  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);
  int client_fd =
      accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
  if (client_fd < 0) {
    return absl::InternalError(absl::StrFormat(
        "Accept failed. errno: %s", absl::base_internal::StrError(errno)));
  }

  auto close_fd = absl::MakeCleanup([client_fd]() { close(client_fd); });

  ABSL_RETURN_IF_ERROR(SetNonBlocking(client_fd));

  epoll_event client_ev{};
  client_ev.events = EPOLLIN | EPOLLET;
  client_ev.data.fd = client_fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to track client fd %d in epoll tree. errno: %s",
                        client_fd, absl::base_internal::StrError(errno)));
  }

  std::move(close_fd).Cancel();
  return absl::OkStatus();
}

absl::Status EpollServer::HandleClientRead(int client_fd) {
  char length_buf[4];
  ssize_t bytes_recieved = recv(client_fd, length_buf, 4, 0);
  if (bytes_recieved < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return absl::OkStatus();
    }
    close(client_fd);
    return absl::InternalError(
        absl::StrFormat("Fatal read on client fd %d. errno: %s", client_fd,
                        absl::base_internal::StrError(errno)));
  } else if (bytes_recieved == 0) {
    LOG(INFO) << "Client FD " << client_fd << " disconnected cleanly.\n";
    close(client_fd);
    return absl::OkStatus();
  }

  uint32_t payload_len = ReadUint32(length_buf);
  if (payload_len > 1024 * 1024) {
    close(client_fd);
    return absl::InternalError(absl::StrFormat(
        "Client fd %d payload exceeded 1MB limit. Evicting client\n",
        client_fd));
  }

  ABSL_ASSIGN_OR_RETURN(std::vector<char> payload,
                        ReadClientPayload(payload_len, client_fd));

  std::string command(payload.begin(), payload.end());
  std::string response = message_handler_(command);

  uint32_t response_len_net = htonl(static_cast<uint32_t>(response.size()));
  send(client_fd, &response_len_net, 4, 0);
  send(client_fd, response.data(), response.size(), 0);

  return absl::OkStatus();
}
}  // namespace kvstore
