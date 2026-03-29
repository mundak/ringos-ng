#pragma once

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)

int puts(const char* string);
int printf(const char* format, ...);
int vprintf(const char* format, va_list arguments);
int snprintf(char* buffer, size_t buffer_size, const char* format, ...);
int vsnprintf(char* buffer, size_t buffer_size, const char* format, va_list arguments);

#ifdef __cplusplus
}
#endif

