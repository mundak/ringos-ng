#include <ringos/process.h>
#include <stdlib.h>

void exit(int exit_status)
{
  ringos_thread_exit(static_cast<uint64_t>(static_cast<uint32_t>(exit_status)));
}
