#pragma once

#include <stddef.h>

void* operator new(size_t size, void* storage) noexcept;
void operator delete(void* address, void* storage) noexcept;
void run_global_constructors();

extern "C" void* memcpy(void* destination, const void* source, size_t length);
extern "C" void* memset(void* destination, int value, size_t length);
