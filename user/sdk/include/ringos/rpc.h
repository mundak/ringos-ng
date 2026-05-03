#pragma once

#include <ringos/handle.h>
#include <ringos/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH 31
#define RINGOS_RPC_MAX_REQUEST_SIZE 256

typedef int32_t syscall_result_t;

typedef syscall_result_t (*ringos_rpc_callback)(const void* request, size_t request_size);

syscall_result_t ringos_rpc_register(const char* name, ringos_rpc_callback callback);
syscall_result_t ringos_rpc_open(const char* name, ringos_handle* out_rpc_handle);
syscall_result_t ringos_rpc_call(ringos_handle handle, const void* request, size_t request_size);
syscall_result_t ringos_rpc_close(ringos_handle handle);

#ifdef __cplusplus
}
#endif
