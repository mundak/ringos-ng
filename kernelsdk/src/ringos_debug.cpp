#include <kernelsdk/kernel_debug.h>

int32_t ringos_debug_log(const char* message)
{
  if (message == nullptr)
  {
    return STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(SYSCALL_DEBUG_LOG, reinterpret_cast<uintptr_t>(message));
}
