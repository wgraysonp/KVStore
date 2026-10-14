#ifndef KVSTORE_SRC_KV_SERVICE_CONSTS_H_
#define KVSTORE_SRC_KV_SERVICE_CONSTS_H_

#include <cstdint>

namespace kvstore {
constexpr int PORT = 8080;
constexpr int MAX_EVENTS = 64;
constexpr int MAX_RPC_ID = 100;
constexpr std::size_t HEADER_INT_SIZE = 4;
constexpr std::size_t MAX_CLIENT_PAYLOAD_SIZE = 1024*1024;
}  // namespace kvstore

#endif // KVSTORE_SRC_KV_SERVICE_CONSTS_H_