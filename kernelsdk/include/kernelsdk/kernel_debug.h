#pragma once

#include <kernelsdk/kernel_syscalls.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t ringos_debug_log(const char* message);

#ifdef __cplusplus
}
#endif
