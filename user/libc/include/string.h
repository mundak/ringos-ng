#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void* destination, const void* source, size_t length);
void* memmove(void* destination, const void* source, size_t length);
void* memset(void* destination, int value, size_t length);
int memcmp(const void* left, const void* right, size_t length);
size_t strlen(const char* string);

#ifdef __cplusplus
}
#endif

