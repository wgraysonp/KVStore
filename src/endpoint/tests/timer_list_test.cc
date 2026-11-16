#include "src/endpoint/timer_list.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <ranges>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"

using ::absl_testing::StatusIs;

namespace endpoint {

class TimerListTest : public ::testing::Test {
 protected:
  std::unique_ptr<TimerList> timer_list_;
  absl::TimeZone utc_ = absl::UTCTimeZone();
  absl::Duration threshold_duration_ = absl::Seconds(10);

  void SetUp() override {
    timer_list_ = std::make_unique<TimerList>(threshold_duration_);
  }
};

TEST_F(TimerListTest, TimerListIsConstructedCorrectly) { SUCCEED(); }

TEST_F(TimerListTest, TimerListAddsNewConnectionCorrectly) {
  int test_client_fd = 5;
  absl::Time connection_time = absl::FromCivil(absl::CivilSecond(), utc_);
  absl::Time expected_expiration_time = connection_time + threshold_duration_;

  ABSL_EXPECT_OK(
      timer_list_->AddNewConnection(test_client_fd, connection_time));

  auto map_view = timer_list_->GetConnectionMapView();

  bool map_has_client_entry =
      std::ranges::any_of(map_view, [test_client_fd](const auto& pair) {
        return pair.first == test_client_fd;
      });

  ASSERT_TRUE(map_has_client_entry);

  const std::list<ConnectionTimer>& connection_list =
      timer_list_->GetConnectionList();

  auto list_it = connection_list.begin();
  while (list_it != connection_list.end()) {
    if (list_it->client_fd == test_client_fd) {
      break;
    }
    list_it++;
  }

  if (list_it == connection_list.end()) {
    FAIL() << "Connnection timer not found in interal list";
  }

  EXPECT_EQ(list_it->timeout_deadline, expected_expiration_time);
}

TEST_F(TimerListTest, AddNewConnectionFailsIfClientFDAlreadyExists) {
  int test_client_fd = 5;
  absl::Time connection_time = absl::FromCivil(absl::CivilSecond(), utc_);

  ABSL_ASSERT_OK(
      timer_list_->AddNewConnection(test_client_fd, connection_time));

  int duplicate_client_fd = 5;
  absl::Time duplicate_connection_time =
      absl::FromCivil(absl::CivilSecond(), utc_) + absl::Seconds(10);

  absl::Status status = timer_list_->AddNewConnection(
      duplicate_client_fd, duplicate_connection_time);

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST_F(TimerListTest, RefreshConnectionCorrectlyRefreshesConnectionTimerEntry) {
  int test_client_fd = 5;
  absl::Time connection_time = absl::FromCivil(absl::CivilSecond(), utc_);

  ABSL_ASSERT_OK(
      timer_list_->AddNewConnection(test_client_fd, connection_time));

  int second_test_client_fd = 6;
  absl::Time second_connection_time =
      absl::FromCivil(absl::CivilSecond(), utc_) + absl::Seconds(10);

  ABSL_ASSERT_OK(timer_list_->AddNewConnection(second_test_client_fd,
                                               second_connection_time));

  // update client fd 5 after the second client was added
  absl::Time update_time = second_connection_time + absl::Seconds(20);

  ABSL_ASSERT_OK(timer_list_->RefreshConnection(test_client_fd, update_time));

  auto map_view = timer_list_->GetConnectionMapView();

  bool map_has_client_entry =
      std::ranges::any_of(map_view, [test_client_fd](const auto& pair) {
        return pair.first == test_client_fd;
      });

  ASSERT_TRUE(map_has_client_entry);

  const std::list<ConnectionTimer>& connection_list =
      timer_list_->GetConnectionList();

  absl::Time expected_timeout_deadline = update_time + threshold_duration_;

  // the entry should have been moved to the back of the list
  EXPECT_EQ(connection_list.back().client_fd, test_client_fd);
  EXPECT_EQ(connection_list.back().timeout_deadline, expected_timeout_deadline);
}

TEST_F(TimerListTest, RefreshConnectionFailsIfConnectionNotFound) {
  int non_existent_client_fd = 5;
  absl::Status status = timer_list_->RefreshConnection(
      non_existent_client_fd, absl::FromCivil(absl::CivilSecond(), utc_));

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(TimerListTest,
       ClearExpiredConnectionsReturnsEmpytListIfNoConnectionsAreExpired) {
  int test_client_fd = 5;
  absl::Time connection_time = absl::FromCivil(absl::CivilSecond(), utc_);
  absl::Time expected_expiration_time = connection_time + threshold_duration_;

  ABSL_ASSERT_OK(
      timer_list_->AddNewConnection(test_client_fd, connection_time));

  // clearance time is before the expected expiration time of the connection
  // just added
  std::vector<int> expired_fds =
      timer_list_->ClearExpiredConnections(connection_time);

  EXPECT_EQ(expired_fds.size(), (std::size_t)0);

  // the ConnectionTimer struct should still be in the list
  auto map_view = timer_list_->GetConnectionMapView();

  bool map_has_client_entry =
      std::ranges::any_of(map_view, [test_client_fd](const auto& pair) {
        return pair.first == test_client_fd;
      });

  ASSERT_TRUE(map_has_client_entry);

  const std::list<ConnectionTimer>& connection_list =
      timer_list_->GetConnectionList();

  auto list_it = connection_list.begin();
  while (list_it != connection_list.end()) {
    if (list_it->client_fd == test_client_fd) {
      break;
    }
    list_it++;
  }

  if (list_it == connection_list.end()) {
    FAIL() << "Connnection timer not found in interal list";
  }

  EXPECT_EQ(list_it->timeout_deadline, expected_expiration_time);
}

TEST_F(TimerListTest,
       ClearExpiredConnectionsCorrectlyDeletesExpiredConnections) {
  int test_client_fd = 5;
  absl::Time connection_time = absl::FromCivil(absl::CivilSecond(), utc_);

  ABSL_ASSERT_OK(
      timer_list_->AddNewConnection(test_client_fd, connection_time));

  int second_test_client_fd = 6;
  absl::Time second_connection_time =
      absl::FromCivil(absl::CivilSecond(), utc_) + absl::Seconds(10);

  ABSL_ASSERT_OK(timer_list_->AddNewConnection(second_test_client_fd,
                                               second_connection_time));

  absl::Time expiration_time = connection_time + threshold_duration_;

  // should delete connection for test_client_fd but not for
  // second_test_client_fd
  std::vector<int> expired_fds =
      timer_list_->ClearExpiredConnections(expiration_time);

  ASSERT_EQ(expired_fds.size(), (std::size_t)1);

  EXPECT_EQ(expired_fds[0], test_client_fd);

  bool test_client_fd_in_map = false;
  bool second_test_client_fd_in_map = false;

  auto map_view = timer_list_->GetConnectionMapView();

  for (auto [fd, unused_it] : map_view) {
    if (fd == test_client_fd) test_client_fd_in_map = true;
    if (fd == second_test_client_fd) second_test_client_fd_in_map = true;
  }

  EXPECT_FALSE(test_client_fd_in_map);
  EXPECT_TRUE(second_test_client_fd_in_map);

  const std::list<ConnectionTimer>& connection_timer_list =
      timer_list_->GetConnectionList();

  ASSERT_EQ(connection_timer_list.size(), (std::size_t)1);

  EXPECT_EQ(connection_timer_list.front().client_fd, second_test_client_fd);
  EXPECT_EQ(connection_timer_list.front().timeout_deadline,
            second_connection_time + threshold_duration_);
}

TEST_F(TimerListTest, DeleteConnectionCorrectlyDeletesConnection) {
  int test_client_fd = 5;
  absl::Time connection_time = absl::FromCivil(absl::CivilSecond(), utc_);

  ABSL_ASSERT_OK(
      timer_list_->AddNewConnection(test_client_fd, connection_time));

  timer_list_->DeleteConnection(test_client_fd);

  const std::list<ConnectionTimer>& connections_list =
      timer_list_->GetConnectionList();

  EXPECT_TRUE(connections_list.empty());

  bool found_connection_in_map = false;

  for (auto [fd, unused_it] : timer_list_->GetConnectionMapView()){
    if (fd == test_client_fd) found_connection_in_map = true;
  }

  EXPECT_FALSE(found_connection_in_map);
}

}  // namespace endpoint