#ifndef ENDPOINT_TIMER_LIST_H_
#define ENDPOINT_TIMER_LIST_H_

#include <list>
#include <ranges>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/time/time.h"

namespace endpoint {

struct ConnectionTimer {
  int client_fd;
  absl::Time timeout_deadline;
};

class TimerList {
 public:
  TimerList(absl::Duration timeout_duration)
      : timeout_duration_(timeout_duration) {};

  TimerList(const TimerList&) = delete;
  TimerList(TimerList&&) = delete;
  TimerList& operator=(const TimerList&) = delete;
  TimerList& operator=(TimerList&&) = delete;

  auto GetConnectionMapView() const {
    return connection_time_map_ | std::views::transform([](const auto& pair) {
             return std::pair<int, std::list<ConnectionTimer>::const_iterator>(
                 pair.first, pair.second);
           });
  }

  const std::list<ConnectionTimer> GetConnectionList() const {
    return connection_time_list_;
  }

  // delete all connections whose expiration time is less than or euqual
  // to threshold time and return the file descriptors. Threshold_time
  // should almost always be absl::Now()
  std::vector<int> ClearExpiredConnections(absl::Time threshold_time);

  // Refresh the expiration time of an existing connection
  // the new expiration time is the update_time + timeout_dureation. Update time
  // is the time the connection was most recently updated or created, and should
  // almost always be absl::Now(). Returns error is client_fd does not exist.
  absl::Status RefreshConnection(int client_fd, absl::Time update_time);

  // Add new connection to the list similarly to RefreshConnection. Returns
  // an error if client_fd already exists in the List
  absl::Status AddNewConnection(int client_fd, absl::Time connection_time);

  // Delete a client connection from the timer list
  void DeleteConnection(int client_fd);

 private:
  absl::Duration timeout_duration_;
  std::list<ConnectionTimer> connection_time_list_;
  absl::flat_hash_map<int, std::list<ConnectionTimer>::iterator>
      connection_time_map_;
};

}  // namespace endpoint

#endif  // ENDPOINT_TIMER_LIST_H_