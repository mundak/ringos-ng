#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__clang__) || defined(__GNUC__)
#define RINGOS_NORETURN __attribute__((noreturn))
#else
#define RINGOS_NORETURN
#endif

#ifdef __cplusplus
}
#endif

