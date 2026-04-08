#include "ringos/rpc.h"

#include <ringos/status.h>
#include <ringos/syscalls.h>

int32_t ringos_rpc_open(const char* endpoint_name, ringos_handle* out_channel_handle)
{
  if (endpoint_name == nullptr || out_channel_handle == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall2(
    RINGOS_SYSCALL_RPC_OPEN,
    reinterpret_cast<uintptr_t>(endpoint_name),
    reinterpret_cast<uintptr_t>(out_channel_handle));
}

int32_t ringos_rpc_call(ringos_handle channel_handle, const ringos_rpc_request* request, ringos_rpc_response* response)
{
  if (channel_handle == RINGOS_HANDLE_INVALID || request == nullptr || response == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    RINGOS_SYSCALL_RPC_CALL,
    static_cast<uintptr_t>(channel_handle),
    reinterpret_cast<uintptr_t>(request),
    reinterpret_cast<uintptr_t>(response));
}

int32_t ringos_rpc_wait(ringos_rpc_request* request)
{
  if (request == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_WAIT, reinterpret_cast<uintptr_t>(request));
}

int32_t ringos_rpc_reply(const ringos_rpc_response* response)
{
  if (response == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_REPLY, reinterpret_cast<uintptr_t>(response));
}
