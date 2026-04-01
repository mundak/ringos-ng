#include <stdint.h>
#include <string.h>

int memcmp(const void* left, const void* right, size_t length)
{
  const uint8_t* const left_bytes = static_cast<const uint8_t*>(left);
  const uint8_t* const right_bytes = static_cast<const uint8_t*>(right);

  for (size_t index = 0; index < length; ++index)
  {
    if (left_bytes[index] != right_bytes[index])
    {
      return static_cast<int>(left_bytes[index]) - static_cast<int>(right_bytes[index]);
    }
  }

  return 0;
}

void* memcpy(void* destination, const void* source, size_t length)
{
  uint8_t* const destination_bytes = static_cast<uint8_t*>(destination);
  const uint8_t* const source_bytes = static_cast<const uint8_t*>(source);

  for (size_t index = 0; index < length; ++index)
  {
    destination_bytes[index] = source_bytes[index];
  }

  return destination;
}

void* memmove(void* destination, const void* source, size_t length)
{
  uint8_t* const destination_bytes = static_cast<uint8_t*>(destination);
  const uint8_t* const source_bytes = static_cast<const uint8_t*>(source);

  if (destination_bytes == source_bytes || length == 0)
  {
    return destination;
  }

  if (destination_bytes < source_bytes)
  {
    for (size_t index = 0; index < length; ++index)
    {
      destination_bytes[index] = source_bytes[index];
    }
  }
  else
  {
    for (size_t index = length; index > 0; --index)
    {
      destination_bytes[index - 1] = source_bytes[index - 1];
    }
  }

  return destination;
}

void* memset(void* destination, int value, size_t length)
{
  uint8_t* const destination_bytes = static_cast<uint8_t*>(destination);
  const uint8_t byte_value = static_cast<uint8_t>(value);

  for (size_t index = 0; index < length; ++index)
  {
    destination_bytes[index] = byte_value;
  }

  return destination;
}

size_t strlen(const char* string)
{
  size_t length = 0;

  while (string[length] != '\0')
  {
    ++length;
  }

  return length;
}
