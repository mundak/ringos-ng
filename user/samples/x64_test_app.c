#include <ringos/sdk.h>
#include <stdint.h>

#if defined(__clang__)
__attribute__((section(".text")))
#endif
static const char RING3_MESSAGE[]
  = "x64 PE64 test app reached ring3";

RINGOS_NORETURN void user_start(void)
{
  (void) ringos_syscall1(RINGOS_SYSCALL_DEBUG_LOG, (uintptr_t) RING3_MESSAGE);
  ringos_thread_exit(0);
}
