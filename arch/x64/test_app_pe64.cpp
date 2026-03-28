#include <stdint.h>

namespace
{
  int32_t ringos_syscall(uint64_t syscall_number, uint64_t argument0)
  {
    uint64_t result = 0;

    __asm__ volatile("syscall" : "=a"(result) : "a"(syscall_number), "D"(argument0) : "rcx", "r11", "memory");

    return static_cast<int32_t>(result);
  }
}

int32_t user_main()
{
  static const char message[] = "x64 PE64 test app reached ring3";
  return ringos_syscall(1, reinterpret_cast<uint64_t>(message));
}

[[noreturn]] void user_start()
{
  const int32_t exit_status = user_main();
  (void) ringos_syscall(2, static_cast<uint32_t>(exit_status));

  while (true)
  {
  }
}

