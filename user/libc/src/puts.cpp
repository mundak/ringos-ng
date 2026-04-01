#include <ringos/debug.h>
#include <stdio.h>

int puts(const char* string)
{
  return ringos_debug_log(string);
}
