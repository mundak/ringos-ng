#include "format.h"

#include "console.h"

namespace ringos
{

  void kprint(const char* str)
  {
    console_write(str);
  }

  void kprint_uint(uint64_t value)
  {
    if (value == 0)
    {
      console_write("0");
      return;
    }

    char buf[21]; // 20 digits max for uint64 + null terminator
    buf[20] = '\0';
    int32_t i = 19;

    while (value > 0)
    {
      buf[i--] = "0123456789"[value % 10];
      value /= 10;
    }

    console_write(buf + i + 1);
  }

  void kprint_hex(uint64_t value)
  {
    static const char hex_digits[] = "0123456789abcdef";

    char buf[19]; // "0x" + 16 hex digits + null terminator
    buf[0] = '0';
    buf[1] = 'x';

    for (int32_t i = 0; i < 16; ++i)
    {
      buf[2 + i] = hex_digits[(value >> (60 - i * 4)) & 0xf];
    }

    buf[18] = '\0';
    console_write(buf);
  }

}
