#pragma once

#include <kernelsdk/kernel_syscalls.h>
#include <stddef.h>

#define RINGOS_RPC_ENDPOINT_NAME_MAX_LENGTH 31
#define RINGOS_RPC_MAX_REQUEST_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*ringos_rpc_callback)(const void* request, size_t request_size);

int32_t ringos_rpc_register(const char* name, ringos_rpc_callback callback);
int32_t ringos_rpc_open(const char* name, ringos_handle* out_rpc_handle);
int32_t ringos_rpc_call(ringos_handle handle, const void* request, size_t request_size);
int32_t ringos_rpc_close(ringos_handle handle);

#ifdef __cplusplus
}
#endif