#include "ringos/process.h"

#include <ringos/syscalls.h>
#include <stdint.h>

RINGOS_NORETURN void ringos_thread_exit(uint64_t exit_status)
{
  (void) ringos_syscall1(RINGOS_SYSCALL_THREAD_EXIT, static_cast<uintptr_t>(exit_status));

  for (;;)
  {
  }
}
