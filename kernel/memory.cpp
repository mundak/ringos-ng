#include "memory.h"

#include <stdint.h>

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