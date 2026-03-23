#include "console.h"
#include "panic.h"

namespace ringos
{

  [[noreturn]] void panic(const char* message)
  {
    console_write("PANIC: ");
    console_write(message);
    console_write("\n");
    while (true)
    {
    }
  }

} // namespace ringos
