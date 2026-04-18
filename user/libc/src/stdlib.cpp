#include <errno.h>
#include <ringos/process.h>
#include <stdlib.h>

void abort(void)
{
  exit(EXIT_FAILURE);
}

int abs(int value)
{
  return value < 0 ? -value : value;
}

div_t div(int numerator, int denominator)
{
  div_t result = {
    .quot = numerator / denominator,
    .rem = numerator % denominator,
  };

  return result;
}

void* calloc(size_t count, size_t size)
{
  if (count != 0 && size > ((size_t) -1) / count)
  {
    errno = ENOMEM;
    return nullptr;
  }

  if (count == 0 || size == 0)
  {
    return nullptr;
  }

  errno = ENOMEM;
  return nullptr;
}

void free(void* pointer)
{
  (void) pointer;
}

long labs(long value)
{
  return value < 0 ? -value : value;
}

ldiv_t ldiv(long numerator, long denominator)
{
  ldiv_t result = {
    .quot = numerator / denominator,
    .rem = numerator % denominator,
  };

  return result;
}

long long llabs(long long value)
{
  return value < 0 ? -value : value;
}

lldiv_t lldiv(long long numerator, long long denominator)
{
  lldiv_t result = {
    .quot = numerator / denominator,
    .rem = numerator % denominator,
  };

  return result;
}

void* malloc(size_t size)
{
  if (size == 0)
  {
    return nullptr;
  }

  errno = ENOMEM;
  return nullptr;
}

void* realloc(void* pointer, size_t size)
{
  if (pointer == nullptr)
  {
    return malloc(size);
  }

  if (size == 0)
  {
    free(pointer);
    return nullptr;
  }

  errno = ENOMEM;
  return nullptr;
}
