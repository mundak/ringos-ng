#include "debug.h"

#include "console.h"

void arch_debug_semihost_write(const char* message);

namespace
{

  void debug_write_prefixed_line(const char* prefix, const char* message)
  {
    console_write(prefix);
    console_write(message);
    console_write("\n");
  }

  void debug_write_semihost_prefixed_line(const char* prefix, const char* message)
  {
    arch_debug_semihost_write(prefix);
    arch_debug_semihost_write(message);
    arch_debug_semihost_write("\n");
  }

}

void arch_debug_break();

void debug_log(const char* message)
{
  debug_write_prefixed_line("[debug] ", message);
}

void debug_semihost_log(const char* message)
{
  debug_write_semihost_prefixed_line("[gdb] ", message);
}

void debug_semihost_self_test()
{
  debug_semihost_log("semihosting self-test");
  debug_break();
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
