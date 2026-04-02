#include <stdlib.h>

#include <ringos/process.h>
namespace
{
  constexpr size_t ATEXIT_HANDLER_CAPACITY = 64;

  using atexit_handler = void (*)();

  atexit_handler g_atexit_handlers[ATEXIT_HANDLER_CAPACITY] {};
  size_t g_atexit_handler_count = 0;

  void run_atexit_handlers()
  {
    while (g_atexit_handler_count > 0)
    {
      const atexit_handler handler = g_atexit_handlers[--g_atexit_handler_count];

      if (handler != nullptr)
      {
        handler();
      }
    }
  }
}

int atexit(void (*function)(void))
{
  if (g_atexit_handler_count >= ATEXIT_HANDLER_CAPACITY)
  {
    return -1;
  }

  g_atexit_handlers[g_atexit_handler_count++] = function;
  return 0;
}

void exit(int exit_status)
{
  run_atexit_handlers();
  ringos_thread_exit(static_cast<uint64_t>(static_cast<uint32_t>(exit_status)));
}
