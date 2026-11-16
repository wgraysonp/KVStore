#include "src/endpoint/timer_list.h"

#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

namespace endpoint {

std::vector<int> TimerList::ClearExpiredConnections(absl::Time threshold_time) {
  std::vector<int> expired_connections;
  if (connection_time_map_.empty()) {
    return expired_connections;
  }
  while (!connection_time_list_.empty() &&
         connection_time_list_.front().timeout_deadline <= threshold_time) {
    expired_connections.emplace_back(connection_time_list_.front().client_fd);
    connection_time_map_.erase(connection_time_list_.front().client_fd);
    connection_time_list_.pop_front();
  }
  return expired_connections;
}

absl::Status TimerList::RefreshConnection(int client_fd,
                                          absl::Time update_time) {
  if (!connection_time_map_.contains(client_fd)) {
    return absl::NotFoundError(absl::StrFormat(
        "Client FD %d not in timeout tracking list", client_fd));
  }

  auto it = connection_time_map_.at(client_fd);
  it->timeout_deadline = update_time + timeout_duration_;
  connection_time_list_.splice(connection_time_list_.end(),
                               connection_time_list_, it);
  return absl::OkStatus();
}

absl::Status TimerList::AddNewConnection(int client_fd,
                                         absl::Time connection_time) {
  auto [map_it, inserted] = connection_time_map_.try_emplace(client_fd);
  if (!inserted) {
    return absl::AlreadyExistsError(absl::StrFormat(
        "Client FD %d already exissts in timeout tracking list", client_fd));
  }

  absl::Time timeout_time = connection_time + timeout_duration_;
  map_it->second = connection_time_list_.emplace(connection_time_list_.end(),
                                                client_fd, timeout_time);
  return absl::OkStatus();
}

void TimerList::DeleteConnection(int client_fd) {
  auto it = connection_time_map_.find(client_fd);
  if (it == connection_time_map_.end()) {
    return;
  }
  connection_time_list_.erase(it->second);
  connection_time_map_.erase(it);
}
}  // namespace endpoint