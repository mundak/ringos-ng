#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ringos_rpc_request
{
  uint64_t operation;
  uintptr_t argument0;
  uintptr_t argument1;
  uintptr_t argument2;
  uintptr_t argument3;
} ringos_rpc_request;

typedef struct ringos_rpc_response
{
  int32_t status;
  uint32_t reserved0;
  uintptr_t value0;
  uintptr_t value1;
  uintptr_t value2;
  uintptr_t value3;
} ringos_rpc_response;

int32_t ringos_rpc_call(const ringos_rpc_request* request, ringos_rpc_response* response);
int32_t ringos_rpc_wait(ringos_rpc_request* request);
int32_t ringos_rpc_reply(const ringos_rpc_response* response);

#ifdef __cplusplus
}
#endif

