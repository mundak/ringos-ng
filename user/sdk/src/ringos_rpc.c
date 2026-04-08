#include <ringos/rpc.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

ringos_rpc_status ringos_rpc_open(const char* endpoint_name, ringos_handle* out_channel_handle)
{
  if (endpoint_name == NULL || out_channel_handle == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall2(RINGOS_SYSCALL_RPC_OPEN, (uintptr_t) endpoint_name, (uintptr_t) out_channel_handle);
}

ringos_rpc_status ringos_rpc_call(
  ringos_handle channel_handle, const ringos_rpc_request* request, ringos_rpc_response* out_response)
{
  if (channel_handle == RINGOS_HANDLE_INVALID || request == NULL || out_response == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    RINGOS_SYSCALL_RPC_CALL, (uintptr_t) channel_handle, (uintptr_t) request, (uintptr_t) out_response);
}

ringos_rpc_status ringos_rpc_wait(ringos_rpc_request* request)
{
  if (request == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_WAIT, (uintptr_t) request);
}

ringos_rpc_status ringos_rpc_reply(const ringos_rpc_response* response)
{
  if (response == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_REPLY, (uintptr_t) response);
}
