#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#if defined(__cplusplus)
#define RINGOS_LIBC_NORETURN [[noreturn]]
#else
#define RINGOS_LIBC_NORETURN _Noreturn
#endif

RINGOS_LIBC_NORETURN void exit(int exit_status);
RINGOS_LIBC_NORETURN void abort(void);

void* malloc(size_t size);
void free(void* pointer);
void* calloc(size_t count, size_t size);
void* realloc(void* pointer, size_t size);

int abs(int value);
long labs(long value);

#undef RINGOS_LIBC_NORETURN

#ifdef __cplusplus
}
#endif

