#include "ringos/rpc.h"

#include <ringos/status.h>
#include <ringos/syscalls.h>
#include <stdint.h>

extern "C" [[noreturn]] void ringos_rpc_complete_trampoline();

syscall_result_t ringos_rpc_register(const char* name, ringos_rpc_callback callback)
{
  if (name == nullptr || callback == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    RINGOS_SYSCALL_RPC_REGISTER,
    reinterpret_cast<uintptr_t>(name),
    reinterpret_cast<uintptr_t>(callback),
    reinterpret_cast<uintptr_t>(&ringos_rpc_complete_trampoline));
}

syscall_result_t ringos_rpc_open(const char* name, ringos_handle* out_rpc_handle)
{
  if (name == nullptr || out_rpc_handle == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall2(
    RINGOS_SYSCALL_RPC_OPEN, reinterpret_cast<uintptr_t>(name), reinterpret_cast<uintptr_t>(out_rpc_handle));
}

syscall_result_t ringos_rpc_call(ringos_handle handle, const void* request, size_t request_size)
{
  if (handle == RINGOS_HANDLE_INVALID || request == nullptr || request_size == 0
      || request_size > RINGOS_RPC_MAX_REQUEST_SIZE)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    RINGOS_SYSCALL_RPC_CALL,
    static_cast<uintptr_t>(handle),
    reinterpret_cast<uintptr_t>(request),
    static_cast<uintptr_t>(request_size));
}

syscall_result_t ringos_rpc_close(ringos_handle handle)
{
  if (handle == RINGOS_HANDLE_INVALID)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_RPC_CLOSE, static_cast<uintptr_t>(handle));
}
