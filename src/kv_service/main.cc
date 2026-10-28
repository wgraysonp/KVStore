#include <string>
#include <memory>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "src/endpoint/client_connection.h"
#include "src/endpoint/connection_manager.h"
#include "src/endpoint/consts.h"
#include "src/endpoint/epoll_server.h"
#include "src/kv_service/handlers/kv_get_handler.h"
#include "src/rpc_framework/rpc_interface.h"
#include "src/rpc_framework/rpc_registry.h"
#include "src/storage/key_value_store.h"
#include "src/storage/write_ahead.h"

static const std::string log_file_path = "/tmp/kv_service_wal.log";

absl::StatusOr<std::unique_ptr<endpoint::ConnectionManager>> InitService() {
  ABSL_ASSIGN_OR_RETURN(std::unique_ptr<kvstore::WriteAheadLog> write_ahead_log,
                        kvstore::WriteAheadLog::CreateLog(log_file_path));
  ABSL_ASSIGN_OR_RETURN(
      std::shared_ptr<kvstore::KVStore> kv_store,
      kvstore::KVStore::CreateKVStore(std::move(write_ahead_log)));

  auto rpc_registry = std::make_shared<rpc::RpcRegistry>();
  rpc_registry->RegisterRpc(
      0, std::make_unique<kvstore::KVGetHandlerFactory>(kv_store));

  auto connection_factory =
      std::make_unique<endpoint::ClientConnectionFactory>();

  auto connection_manager = std::make_unique<endpoint::ConnectionManager>(
      endpoint::PORT, std::move(rpc_registry), std::move(connection_factory));
  
  return connection_manager;
}

absl::Status StartAndRunServer() {
  
  ABSL_ASSIGN_OR_RETURN(auto connection_manager, InitService());

  ABSL_RETURN_IF_ERROR(connection_manager->BindAndListen());

  ABSL_RETURN_IF_ERROR(connection_manager->LoopForever());

  return absl::OkStatus();
}

int main() {
  LOG(INFO) << "Starting service... ";
  absl::Status status = StartAndRunServer();
  if (!status.ok()) {
    LOG(ERROR) << "Server exited premeturly\n";
    return 1;
  }
  return 0;
}