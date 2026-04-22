#pragma once

#include <kernelsdk/kernel_syscalls.h>

#define RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH 31

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

typedef int32_t (*ringos_rpc_callback)(const ringos_rpc_request* request);

int32_t ringos_rpc_register(const char* name, ringos_rpc_callback callback);
int32_t ringos_rpc_open(const char* name, ringos_handle* out_rpc_handle);
int32_t ringos_rpc_call(ringos_handle handle, const ringos_rpc_request* request);
int32_t ringos_rpc_close(ringos_handle handle);

#ifdef __cplusplus
}
#endif