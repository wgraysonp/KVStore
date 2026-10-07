#include "src/storage/key_value_store.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "src/storage/tests/mock_write_ahead_log.h"

using ::absl_testing::StatusIs;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::NiceMock;

using InternalKVMap = absl::flat_hash_map<std::string, std::string>;

namespace kvstore {

class KeyValueStoreTests : public ::testing::Test {
 protected:
  NiceMock<MockWriteAheadLog>* mock_log_;
  std::unique_ptr<KVStore> store_;

  void CreateTestKVStore(const InternalKVMap& initial_kv_map) {
    auto mock_log_unique_ptr = std::make_unique<NiceMock<MockWriteAheadLog>>();
    mock_log_ = mock_log_unique_ptr.get();

    ON_CALL(*mock_log_, RecoverKVStore()).WillByDefault(Return(initial_kv_map));

    absl::StatusOr<std::unique_ptr<KVStore>> create_store_status =
        KVStore::CreateKVStore(std::move(mock_log_unique_ptr));

    ABSL_ASSERT_OK(create_store_status);

    store_ = std::move(create_store_status.value());
  }
};

TEST_F(KeyValueStoreTests, KVStoreCreationFailsIfMapRecoveryFails) {
  auto mock_log_unique_ptr = std::make_unique<NiceMock<MockWriteAheadLog>>();
  mock_log_ = mock_log_unique_ptr.get();

  absl::Status recovery_failure_status =
      absl::InternalError("Failure recovering map from WAL.");

  EXPECT_CALL(*mock_log_, RecoverKVStore())
      .Times(1)
      .WillOnce(Return(recovery_failure_status));

  absl::StatusOr<std::unique_ptr<KVStore>> create_store_status =
      KVStore::CreateKVStore(std::move(mock_log_unique_ptr));

  EXPECT_THAT(create_store_status,
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failure recovering map from WAL.")));
}

TEST_F(KeyValueStoreTests, KVStoreIsSuccessfullyCreatedFromEmptyLog) {
  ASSERT_NO_FATAL_FAILURE(CreateTestKVStore(InternalKVMap{}))
      << "KV Store creation failed";

  const InternalKVMap& internal_map = store_->GetKVMap();

  EXPECT_THAT(internal_map, ContainerEq(InternalKVMap{}));
}

TEST_F(KeyValueStoreTests, KVStoreCorrectlyAddsSingleKeyValuePair) {
  ASSERT_NO_FATAL_FAILURE(CreateTestKVStore(InternalKVMap{}))
      << "KV Store creation failed";

  const InternalKVMap& internal_map = store_->GetKVMap();

  EXPECT_THAT(internal_map, ContainerEq(InternalKVMap{}));

  Request req{
      .request_type = RequestType::PUT,
      .key = "key_1",
      .value = "value_1",
  };

  Response resp = store_->ProcessRequest(req);

  EXPECT_EQ(resp.response_status, ResponseStatus::Ok);
  EXPECT_EQ(resp.value, std::nullopt);

  EXPECT_THAT(store_->GetKVMap(),
              ContainerEq(InternalKVMap{{"key_1", "value_1"}}));
}

TEST_F(KeyValueStoreTests,
       KVStoreReturnsResponseWithErrorMessageIfLogWriteFails) {
  ASSERT_NO_FATAL_FAILURE(CreateTestKVStore(InternalKVMap{}))
      << "KV Store creation failed";

  const InternalKVMap& internal_map = store_->GetKVMap();

  EXPECT_THAT(internal_map, ContainerEq(InternalKVMap{}));

  Request req{
      .request_type = RequestType::PUT,
      .key = "key_1",
      .value = "value_1",
  };

  EXPECT_CALL(*mock_log_, LogRequest(req))
      .Times(1)
      .WillOnce(Return(absl::InternalError("Some horrific error")));

  Response resp = store_->ProcessRequest(req);

  EXPECT_EQ(resp, Response{.response_status = ResponseStatus::InternalError});
}

TEST_F(KeyValueStoreTests,
       KVStoreReturnsCorrectErrorWithGetRequestForNonExistentKey) {
  ASSERT_NO_FATAL_FAILURE(CreateTestKVStore(InternalKVMap{}))
      << "KV Store creation failed";

  const InternalKVMap& internal_map = store_->GetKVMap();

  EXPECT_THAT(internal_map, ContainerEq(InternalKVMap{}));

  Request req{
      .request_type = RequestType::GET,
      .key = "key_1",
  };

  Response resp = store_->ProcessRequest(req);

  EXPECT_EQ(resp,
            Response{.response_status = ResponseStatus::KeyNotFoundError});
}

TEST_F(KeyValueStoreTests,
       KVStoreReturnsCorrectValueWhenKeyExistsOnGetRequest) {
  ASSERT_NO_FATAL_FAILURE(
      CreateTestKVStore(InternalKVMap{{"key_1", "value_1"}}))
      << "KV Store creation failed";

  const InternalKVMap& internal_map = store_->GetKVMap();

  EXPECT_THAT(internal_map, ContainerEq(InternalKVMap{{"key_1", "value_1"}}));

  Request req{
      .request_type = RequestType::GET,
      .key = "key_1",
  };

  Response resp = store_->ProcessRequest(req);
  Response expected_resp{
      .response_status = ResponseStatus::Ok,
      .value = "value_1",
  };

  EXPECT_EQ(resp, expected_resp);
}

// test multiple threads reading and writing to the kv store
// this should be run with thread sanitizer (bazel test // ... -config=tsan)
TEST_F(KeyValueStoreTests, ConcurrencyStressTest) {
  ASSERT_NO_FATAL_FAILURE(CreateTestKVStore(InternalKVMap{{"key", "value"}}))
      << "KV Store creation failed";

  std::atomic<bool> hold_readers(true);

  Request reader_request{
      .request_type = RequestType::GET,
      .key = "key",
  };

  std::vector<std::thread> threads;

  // add reader threads
  for (int i = 0; i < 5; i++) {
    threads.emplace_back([&, i]() {
      while (hold_readers.load()) {
        Response resp = store_->ProcessRequest(reader_request);
      }
    });
  }

  // add writer threads
  for (int i = 0; i < 3; i++) {
    Request req{
        .request_type = RequestType::PUT,
        .key = "key_" + std::to_string(i),
        .value = "value_" + std::to_string(i),
    };
    threads.emplace_back(
        [&, i, req]() { Response resp = store_->ProcessRequest(req); });
  }

  hold_readers = false;
  for (auto& thread : threads) {
    thread.join();
  }

  const InternalKVMap& internal_map = store_->GetKVMap();

  InternalKVMap expected_map{{"key", "value"},
                             {"key_0", "value_0"},
                             {"key_1", "value_1"},
                             {"key_2", "value_2"}};

  EXPECT_THAT(internal_map, ContainerEq(expected_map));
}

}  // namespace kvstore
