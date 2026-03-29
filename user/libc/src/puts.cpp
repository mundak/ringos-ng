#include <stdio.h>

#include <ringos/debug.h>

int puts(const char* string)
{
  return ringos_debug_log(string);
}
