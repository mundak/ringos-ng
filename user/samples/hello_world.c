#include <stdio.h>

int main(void)
{
  if (puts("hello world from ANSI C") == EOF)
  {
    return 1;
  }

  return 0;
}

