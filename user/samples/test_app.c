#include <stdio.h>
#include <stdlib.h>

static const char USER_MESSAGE[] = "generic test app reached user mode";

int main(void)
{
  puts(USER_MESSAGE);
  return EXIT_SUCCESS;
}
