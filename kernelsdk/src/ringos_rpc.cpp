#include <kernelsdk/kernel_rpc.h>

extern "C" [[noreturn]] void ringos_rpc_complete_trampoline();

int32_t ringos_rpc_register(const char* name, ringos_rpc_callback callback)
{
  if (name == nullptr || callback == nullptr)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    SYSCALL_RPC_REGISTER,
    reinterpret_cast<uintptr_t>(name),
    reinterpret_cast<uintptr_t>(callback),
    reinterpret_cast<uintptr_t>(&ringos_rpc_complete_trampoline));
}

int32_t ringos_rpc_open(const char* name, ringos_handle* out_rpc_handle)
{
  if (name == nullptr || out_rpc_handle == nullptr)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall2(
    SYSCALL_RPC_OPEN, reinterpret_cast<uintptr_t>(name), reinterpret_cast<uintptr_t>(out_rpc_handle));
}

int32_t ringos_rpc_call(ringos_handle handle, const void* request, size_t request_size)
{
  if (handle == RINGOS_HANDLE_INVALID || request == nullptr || request_size == 0
      || request_size > RINGOS_RPC_MAX_REQUEST_SIZE)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall3(
    SYSCALL_RPC_CALL,
    static_cast<uintptr_t>(handle),
    reinterpret_cast<uintptr_t>(request),
    static_cast<uintptr_t>(request_size));
}

int32_t ringos_rpc_close(ringos_handle handle)
{
  if (handle == RINGOS_HANDLE_INVALID)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(SYSCALL_RPC_CLOSE, static_cast<uintptr_t>(handle));
}
