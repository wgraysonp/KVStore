#ifndef SRC_RPC_FRAMEWORK_DATA_H_
#define SRC_RPC_FRAMEWORK_DATA_H_

#include <cstdint>
#include <vector>

namespace rpc {

struct RawMessage {
  uint32_t rpc_id;
  uint32_t payload_size;
  std::string request_payload;
};

}  // namespace rpc

#endif  // SRC_RPC_FRAMEWORK_DATA_H_