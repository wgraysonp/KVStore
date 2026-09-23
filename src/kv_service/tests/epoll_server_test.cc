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
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

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

    server = std::make_unique<EpollServer>(8080, greeting, true);
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

  int ClientConnect() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
      ADD_FAILURE() << "Failed to create client socket descriptor";
      return -1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <
        0) {
      ADD_FAILURE() << "Failed to connect client socket";
      return -1;
    }
    return client_fd;
  }

  uint32_t ReadUint32(char* data) {
    uint32_t num;
    std::memcpy(&num, data, 4);
    return ntohl(num);
  }
};

// Test that the loop shuts down smoothly on command
TEST_F(EpollServerTest, ExitsCleanlyWithOkStatusOnStop) {
  SUCCEED();  // Place holder to register the test run
  // the loop exit status should be ok in TearDown()
}

// Server handles a single client connection and responds with the correct
// message
TEST_F(EpollServerTest, ServerHandlesSingleClientConnection) {
  int client_fd = ClientConnect();
  ASSERT_GE(client_fd, 0);

  std::string message = "Grayson";

  uint32_t message_len = htonl(static_cast<uint32_t>(message.size()));
  send(client_fd, &message_len, 4, 0);
  send(client_fd, message.data(), message.size(), 0);

  std::string expected_response = "Hello Grayson";
  uint32_t expected_response_bytes = expected_response.size();

  uint32_t expected_header_byte_size = 4;
  std::vector<char> size_header(expected_header_byte_size);

  uint32_t header_byte_size = recv(client_fd, size_header.data(), 4, 0);
  EXPECT_EQ(header_byte_size, expected_header_byte_size);

  uint32_t header_size = ReadUint32(size_header.data());
  EXPECT_EQ(header_size, expected_response_bytes);

  std::vector<char> resp(expected_response_bytes);

  uint32_t bytes_recieved =
      recv(client_fd, resp.data(), expected_response_bytes, 0);

  EXPECT_EQ(bytes_recieved, expected_response_bytes);
  EXPECT_EQ(std::string(resp.begin(), resp.end()), expected_response);

  close(client_fd);
}

TEST_F(EpollServerTest, ServerHandlesMultipleClientConnections) {
  struct TestResult {
    uint32_t header_byte_size;
    uint32_t header_size;
    uint32_t bytes_received;
    std::string response;
  };

  int num_clients = 50;
  std::vector<std::thread> clients;
  std::vector<TestResult> results;
  std::vector<TestResult> expected_results;

  results.resize(num_clients);
  expected_results.resize(num_clients);

  for (int i = 0; i < num_clients; i++) {
    int client_fd = ClientConnect();
    ASSERT_GE(client_fd, 0);
    std::string message = absl::StrFormat("client %d", i);
    std::string response = absl::StrCat("Hello ", message);

    expected_results[i].header_byte_size = 4;
    expected_results[i].header_size = response.size();
    expected_results[i].bytes_received = response.size();
    expected_results[i].response = std::move(response);

    clients.emplace_back([this, i, client_fd, message, &results]() {
      // force the thread to sleep so that we can saturate the epoll
      // tree with multiple connections before the threads can disconnect
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      uint32_t message_len = htonl(static_cast<uint32_t>(message.size()));
      send(client_fd, &message_len, 4, 0);
      send(client_fd, message.data(), message.size(), 0);

      std::vector<char> size_header(4);

      uint32_t header_byte_size = recv(client_fd, size_header.data(), 4, 0);

      uint32_t header_size = ReadUint32(size_header.data());

      std::vector<char> resp(header_size);

      uint32_t bytes_recieved = recv(client_fd, resp.data(), header_size, 0);

      results[i].header_byte_size = header_byte_size;
      results[i].header_size = header_size;
      results[i].bytes_received = bytes_recieved;
      results[i].response = std::string(resp.begin(), resp.end());

      close(client_fd);
    });
  }

  for (auto& t : clients) {
    if (t.joinable()) {
      t.join();
    }
  }

  for (int i = 0; i < num_clients; i++) {
    SCOPED_TRACE("Evaluating results for Client Index: " + std::to_string(i));

    EXPECT_EQ(results[i].header_byte_size,
              expected_results[i].header_byte_size);
    EXPECT_EQ(results[i].header_size, expected_results[i].header_size);
    EXPECT_EQ(results[i].bytes_received, expected_results[i].bytes_received);
    EXPECT_EQ(results[i].response, expected_results[i].response);
  }
}
