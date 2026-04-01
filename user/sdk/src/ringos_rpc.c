#include <ringos/rpc.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

int32_t ringos_rpc_call(const ringos_rpc_request* request, ringos_rpc_response* response)
{
  if (request == NULL || response == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall2(RINGOS_SYSCALL_RPC_CALL, (uintptr_t) request, (uintptr_t) response);
}

int32_t ringos_rpc_wait(ringos_rpc_request* request)
{
  if (request == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_WAIT, (uintptr_t) request);
}

int32_t ringos_rpc_reply(const ringos_rpc_response* response)
{
  if (response == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_REPLY, (uintptr_t) response);
}
