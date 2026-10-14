#ifndef ENDPOINT_CONSTS_H_
#define ENDPOINT_CONSTS_H_

#include <cstdint>

namespace endpoint {
constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 64;
constexpr int MAX_RPC_ID = 100;
constexpr std::size_t HEADER_INT_SIZE = 4;
constexpr std::size_t MAX_CLIENT_PAYLOAD_SIZE = 1024 * 1024;
}  // namespace endpoint

#endif  // ENDPOINT_CONSTS_H_