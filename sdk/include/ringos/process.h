#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

RINGOS_NORETURN void ringos_thread_exit(uint64_t exit_status);

#ifdef __cplusplus
}
#endif
