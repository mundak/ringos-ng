#include <stdio.h>

#include <ringos/debug.h>
#include <string.h>

int puts(const char* string)
{
  if (string == nullptr)
  {
    return EOF;
  }

  if (ringos_debug_log(string) < 0)
  {
    return EOF;
  }

  return ringos_debug_log("\n") < 0 ? EOF : static_cast<int>(strlen(string) + 1);
}
