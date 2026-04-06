#include <ringos/debug.h>
#include <ringos/status.h>

int main(void)
{
  if (ringos_debug_log("hello world from ANSI C") != RINGOS_STATUS_OK)
  {
    return 1;
  }

  return 0;
}
