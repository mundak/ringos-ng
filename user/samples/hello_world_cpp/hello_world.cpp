#include <cstdint>
#include <stdio.h>
#include <type_traits>

namespace
{
  int32_t g_status = 1;

  class greeting_initializer
  {
  public:
    greeting_initializer() { g_status = std::is_integral<int32_t>::value ? 0 : 1; }
  };

  greeting_initializer g_initializer {};
}

int main()
{
  if (g_status != 0)
  {
    return g_status;
  }

  if (puts("hello world from libc++") == EOF)
  {
    return 1;
  }

  return 0;
}
