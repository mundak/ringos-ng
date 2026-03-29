#include <stdint.h>

namespace
{
  constexpr uint16_t DEBUGCON_PORT = 0xE9;

  void outb(uint16_t port, uint8_t value)
  {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
  }
}

void arch_debug_semihost_write(const char* message)
{
  if (message == nullptr)
  {
    return;
  }

  while (*message != '\0')
  {
    outb(DEBUGCON_PORT, static_cast<uint8_t>(*message));
    ++message;
  }
}

void arch_debug_break()
{
  asm volatile("int3");
}
