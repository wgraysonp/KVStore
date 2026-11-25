#ifndef ENDPOINT_CONSTS_H_
#define ENDPOINT_CONSTS_H_

#include <cstdint>

#include "absl/time/time.h"

namespace common {
constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 64;
constexpr int MAX_RPC_ID = 100;
constexpr int DEFAULT_VAL_SIZE = 20;
constexpr int MAX_KEY_SIZE = 128;
constexpr int READ_BUFFER_SIZE = 1024;
constexpr std::size_t HEADER_INT_SIZE = sizeof(uint32_t);
constexpr std::size_t MAX_CLIENT_PAYLOAD_SIZE = 1024 * 1024;
constexpr absl::Duration CLIENT_TIMEOUT_THRESHOLD = absl::Minutes(1);
}  // namespace common

#endif  // ENDPOINT_CONSTS_H_