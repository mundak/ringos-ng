#include <kernelsdk/kernel_debug.h>

int main(void)
{
  if (ringos_debug_log("hello world from ANSI C") != STATUS_OK)
  {
    return 1;
  }

  return 0;
}
