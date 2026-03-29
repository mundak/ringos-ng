#include <ringos/sdk.h>
#include <stdint.h>

static const char USER_MESSAGE[] = "generic test app reached user mode";

RINGOS_NORETURN void user_start(void)
{
  (void) ringos_syscall1(RINGOS_SYSCALL_DEBUG_LOG, (uintptr_t) USER_MESSAGE);
  ringos_thread_exit(0);
}
