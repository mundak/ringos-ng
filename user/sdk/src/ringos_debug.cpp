#include "ringos/debug.h"

#include <ringos/status.h>
#include <ringos/syscalls.h>
#include <stdint.h>

int32_t ringos_debug_log(const char* message)
{
  if (message == nullptr)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_DEBUG_LOG, reinterpret_cast<uintptr_t>(message));
}
