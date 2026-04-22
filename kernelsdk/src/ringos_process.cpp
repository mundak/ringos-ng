#include <kernelsdk/kernel_syscalls.h>

__attribute__((noreturn)) void ringos_thread_exit(uint64_t exit_status)
{
  (void) ringos_syscall1(SYSCALL_THREAD_EXIT, static_cast<uintptr_t>(exit_status));

  for (;;)
  {
  }
}
