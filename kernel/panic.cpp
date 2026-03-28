#include "panic.h"

#include "debug.h"

[[noreturn]] void panic(const char* message)
{
  debug_semihost_log("PANIC");
  debug_semihost_log(message);
  while (true)
  {
  }
}
