#pragma once

#include <ringos/handle.h>
#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH 31

typedef int32_t syscall_result_t;

typedef struct ringos_rpc_request
{
  uint64_t operation;
  uintptr_t argument0;
  uintptr_t argument1;
  uintptr_t argument2;
  uintptr_t argument3;
} ringos_rpc_request;

typedef syscall_result_t (*ringos_rpc_callback)(const ringos_rpc_request* request);

syscall_result_t ringos_rpc_register(const char* name, ringos_rpc_callback callback);
syscall_result_t ringos_rpc_open(const char* name, ringos_handle* out_rpc_handle);
syscall_result_t ringos_rpc_call(ringos_handle handle, const ringos_rpc_request* request);
syscall_result_t ringos_rpc_close(ringos_handle handle);

#ifdef __cplusplus
}
#endif
