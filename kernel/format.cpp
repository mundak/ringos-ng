#include "format.h"

#include "console.h"

void kprint(const char* str)
{
  console_write(str);
}
