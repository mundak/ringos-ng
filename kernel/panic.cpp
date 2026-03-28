#include "panic.h"

#include "debug.h"

[[noreturn]] void panic(const char* message)
{
  debug_log("PANIC");
  debug_log(message);
  while (true)
  {
  }
}
