#include <csignal>
#include <functional>
#include <iostream>
#include <string>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/str_format.h"
#include "src/endpoint/consts.h"
#include "src/endpoint/epoll_server.h"

using namespace endpoint;

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
  LOG(INFO) << "Starting service... ";
  absl::Status status = StartAndRunServer();
  if (!status.ok()) {
    LOG(ERROR) << "Server exited premeturly\n";
    return 1;
  }
  return 0;
}