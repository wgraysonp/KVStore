#ifndef ENDPOINT_CONSTS_H_
#define ENDPOINT_CONSTS_H_

#include <cstdint>

namespace common {
constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 64;
constexpr int MAX_RPC_ID = 100;
constexpr int DEFAULT_VAL_SIZE = 20;
constexpr int MAX_KEY_SIZE = 128;
constexpr std::size_t HEADER_INT_SIZE = sizeof(uint32_t);
constexpr std::size_t MAX_CLIENT_PAYLOAD_SIZE = 1024 * 1024;
}  // namespace common

#endif  // ENDPOINT_CONSTS_H_