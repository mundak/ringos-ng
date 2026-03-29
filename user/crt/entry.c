#include <ringos/process.h>

#include <stdint.h>

int main(void);

RINGOS_NORETURN void user_start(void)
{
  const int32_t main_status = main();
  ringos_thread_exit((uint64_t) (int64_t) main_status);
}

