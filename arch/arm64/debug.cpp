#include <stdint.h>

namespace
{
  constexpr uintptr_t SYS_WRITE0 = 0x04;

}

void arch_debug_semihost_write(const char* message)
{
  if (message == nullptr)
  {
    return;
  }

  register uintptr_t operation asm("x0") = SYS_WRITE0;
  register const char* parameter asm("x1") = message;
  asm volatile("hlt #0xF000" : "+r"(operation) : "r"(parameter) : "memory");
}

void arch_debug_break()
{
  asm volatile("brk #0");
}
