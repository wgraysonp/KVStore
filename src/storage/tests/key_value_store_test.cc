#include "src/storage/key_value_store.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/storage/tests/mock_write_ahead_log.h"

class KeyValueStoreTests : public ::testing::Test {};

TEST_F(KeyValueStoreTests, TestTest) { SUCCEED(); }
