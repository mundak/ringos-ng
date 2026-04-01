#include "memory.h"

#include <stdint.h>

extern "C" int __cxa_atexit(void (*)(void*), void*, void*)
{
  return 0;
}

void* operator new(size_t, void* storage) noexcept
{
  return storage;
}

namespace
{
  using init_function = void (*)();
}

extern "C" init_function __init_array_start[];
extern "C" init_function __init_array_end[];

void operator delete(void*, void*) noexcept { }

void run_global_constructors()
{
  for (init_function* current = __init_array_start; current != __init_array_end; ++current)
  {
    (*current)();
  }
}

extern "C" void* memcpy(void* destination, const void* source, size_t length)
{
  uint8_t* destination_bytes = reinterpret_cast<uint8_t*>(destination);
  const uint8_t* source_bytes = reinterpret_cast<const uint8_t*>(source);

  for (size_t index = 0; index < length; ++index)
  {
    destination_bytes[index] = source_bytes[index];
  }

  return destination;
}

extern "C" void* memset(void* destination, int value, size_t length)
{
  uint8_t* destination_bytes = reinterpret_cast<uint8_t*>(destination);
  const uint8_t fill_byte = static_cast<uint8_t>(value);

  for (size_t index = 0; index < length; ++index)
  {
    destination_bytes[index] = fill_byte;
  }

  return destination;
}
