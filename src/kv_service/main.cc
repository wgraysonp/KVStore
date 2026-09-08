#include <functional>
#include <string>
#include <iostream>
#include <csignal>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/str_format.h"
#include "src/kv_service/consts.h"
#include "src/kv_service/epoll_server.h"

using namespace kvstore;

absl::Status StartAndRunServer() {
  std::function<std::string(const std::string&)> greeting =
      [](const std::string& name) -> std::string {
    return absl::StrFormat("Hello %s", name);
  };

  EpollServer server(PORT, greeting, false);

  ABSL_RETURN_IF_ERROR(server.Start());
  ABSL_RETURN_IF_ERROR(server.RunLoop());

  return absl::OkStatus();
}

int main() {
  std::signal(SIGPIPE, SIG_IGN);
  LOG(INFO) << "Starting service... ";
  absl::Status status = StartAndRunServer();
  if (!status.ok()) {
    LOG(ERROR) << "Server exited premeturly\n";
    return 1;
  }

  return 0;
}