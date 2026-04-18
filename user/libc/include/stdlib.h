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

typedef struct div_t
{
  int quot;
  int rem;
} div_t;

typedef struct ldiv_t
{
  long quot;
  long rem;
} ldiv_t;

typedef struct lldiv_t
{
  long long quot;
  long long rem;
} lldiv_t;

RINGOS_LIBC_NORETURN void exit(int exit_status);
RINGOS_LIBC_NORETURN void abort(void);
int atexit(void (*function)(void));

void* malloc(size_t size);
void free(void* pointer);
void* calloc(size_t count, size_t size);
void* realloc(void* pointer, size_t size);

int abs(int value);
long labs(long value);
long long llabs(long long value);
div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
lldiv_t lldiv(long long numerator, long long denominator);

#undef RINGOS_LIBC_NORETURN

#ifdef __cplusplus
}
#endif
