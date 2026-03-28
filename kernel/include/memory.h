#pragma once

#include <stddef.h>

extern "C" void* memcpy(void* destination, const void* source, size_t length);
extern "C" void* memset(void* destination, int value, size_t length);