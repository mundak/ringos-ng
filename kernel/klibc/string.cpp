#include "string.h"

void copy_string(char* destination, size_t capacity, const char* source)
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  size_t index = 0;

  while (source != nullptr && source[index] != '\0' && index + 1 < capacity)
  {
    destination[index] = source[index];
    ++index;
  }

  destination[index] = '\0';
}
