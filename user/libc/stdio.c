#include <stdio.h>

#include <ringos/debug.h>
#include <ringos/status.h>

#include <stddef.h>

int puts(const char* string)
{
  if (string == NULL)
  {
    return EOF;
  }

  // The current user console is line-oriented, so one puts call maps directly
  // to one kernel debug log line.
  if (ringos_debug_log(string) != RINGOS_STATUS_OK)
  {
    return EOF;
  }

  return 0;
}

