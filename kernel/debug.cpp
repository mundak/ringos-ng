#include "debug.h"

#include "console.h"

namespace
{

  void debug_write_prefixed_line(const char* prefix, const char* message)
  {
    console_write(prefix);
    console_write(message);
    console_write("\n");
  }

}

void arch_debug_break();

void debug_log(const char* message)
{
  debug_write_prefixed_line("[debug] ", message);
}

void debug_break()
{
  arch_debug_break();
}

void debug_break(const char* reason)
{
  debug_log(reason);
  debug_break();
}
