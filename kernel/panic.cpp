#include "panic.h"

#include "console.h"

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

}
