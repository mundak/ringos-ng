#pragma once

#include <stdint.h>

#define TERMINAL_SERVICE_RPC_ENDPOINT_NAME "srv_terminal"

enum terminal_service_rpc_operation : uint64_t
{
  TERMINAL_SERVICE_RPC_OPERATION_WRITE_CHARACTER = 1,
};
