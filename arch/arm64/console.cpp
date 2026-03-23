#include "console.h"

#include "pl011.h"

void console_write(const char* str)
{
  pl011_puts(str);
}
