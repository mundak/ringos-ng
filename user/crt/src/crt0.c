#include <stdlib.h>

extern int main(void);

typedef void(__cdecl* ringos_initializer)(void);

#pragma section(".CRT$XIA", long, read)
#pragma section(".CRT$XIZ", long, read)
#pragma section(".CRT$XCA", long, read)
#pragma section(".CRT$XCZ", long, read)

__declspec(allocate(".CRT$XIA")) ringos_initializer __xi_a[] = { 0 };
__declspec(allocate(".CRT$XIZ")) ringos_initializer __xi_z[] = { 0 };
__declspec(allocate(".CRT$XCA")) ringos_initializer __xc_a[] = { 0 };
__declspec(allocate(".CRT$XCZ")) ringos_initializer __xc_z[] = { 0 };

static void run_initializers(ringos_initializer* first, ringos_initializer* last)
{
  for (ringos_initializer* current = first; current != last; ++current)
  {
    if (*current != 0)
    {
      (*current)();
    }
  }
}

void user_start(void)
{
  run_initializers(__xi_a, __xi_z);
  run_initializers(__xc_a, __xc_z);
  exit(main());
}
