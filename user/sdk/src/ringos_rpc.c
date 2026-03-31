#include <ringos/rpc.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>

int32_t ringos_rpc_call(uint64_t rpc_handle, const ringos_rpc_request* request, ringos_rpc_response* response)
{
  if (rpc_handle == RINGOS_HANDLE_INVALID || request == NULL || response == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(RINGOS_SYSCALL_RPC_CALL, (uintptr_t) rpc_handle, (uintptr_t) request, (uintptr_t) response);
}
