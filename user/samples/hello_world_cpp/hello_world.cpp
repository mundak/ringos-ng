#include <ringos/debug.h>
#include <ringos/status.h>

#include <cstdint>
#include <type_traits>

namespace
{
  int32_t g_status = 1;

  class greeting_initializer
  {
  public:
    greeting_initializer()
    {
      g_status = std::is_integral<int32_t>::value ? 0 : 1;
    }
  };

  greeting_initializer g_initializer {};
}

int main()
{
  if (g_status != 0)
  {
    return g_status;
  }

  if (ringos_debug_log("hello world from libc++") != RINGOS_STATUS_OK)
  {
    return 1;
  }

  return 0;
}
