#include "src/kv_service/epoll_server.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"

using namespace kvstore;

class EpollServerTest : public ::testing::Test {
 protected:
  std::unique_ptr<EpollServer> server;
  std::thread server_thread;
  absl::Status loop_exit_status;

  void SetUp() override {
    std::function<std::string(const std::string&)> greeting =
        [](const std::string& name) -> std::string {
      return absl::StrFormat("Hello %s", name);
    };

    server = std::make_unique<EpollServer>(0, greeting, false);
    ABSL_ASSERT_OK(server->Start());

    server_thread =
        std::thread([this]() { loop_exit_status = server->RunLoop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  void TearDown() override {
    server->Stop();

    if (server_thread.joinable()) {
      server_thread.join();
    }

    ABSL_EXPECT_OK(loop_exit_status)
        << "Server exited with error: " << loop_exit_status.ToString();
  }
};

// Test that the loop shuts down smoothly on command
TEST_F(EpollServerTest, ExitsCleanlyWithOkStatusOnStop) {
  SUCCEED();  // Place holder to register the test run
  // the loop exit status should be ok in TearDown()
}
