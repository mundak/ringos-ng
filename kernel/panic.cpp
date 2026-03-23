#include "panic.h"

#include "console.h"

[[noreturn]] void panic(const char* message)
{
  console_write("PANIC: ");
  console_write(message);
  console_write("\n");
  while (true)
  {
  }
}
