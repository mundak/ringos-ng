#pragma once

#include <stddef.h>
#include <stdint.h>

#define TERMINAL_SERVICE_RPC_ENDPOINT_NAME "srv_terminal"

#define TERMINAL_SERVICE_WRITE_MAX_BYTES 240

enum terminal_service_rpc_operation : uint64_t
{
  TERMINAL_SERVICE_RPC_OPERATION_WRITE_BUFFER = 1,
};

typedef struct terminal_service_request_header
{
  uint64_t operation;
} terminal_service_request_header;

typedef struct terminal_service_write_request
{
  terminal_service_request_header header;
  uint64_t length;
  uint8_t bytes[TERMINAL_SERVICE_WRITE_MAX_BYTES];
} terminal_service_write_request;
