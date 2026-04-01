#include <ringos/debug.h>
#include <ringos/status.h>
#include <ringos/syscalls.h>
#include <stddef.h>
#include <stdint.h>

int32_t ringos_debug_log(const char* message)
{
  if (message == NULL)
  {
    return RINGOS_STATUS_INVALID_ARGUMENT;
  }

  return ringos_syscall1(RINGOS_SYSCALL_DEBUG_LOG, (uintptr_t) message);
}
