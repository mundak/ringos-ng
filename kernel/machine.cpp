#include "machine.h"

#include "arch_machine.h"
#include "klibc/memory.h"
#include "panic.h"

namespace
{
  machine_descriptor g_machine {};
}

void initialize_machine(const boot_info& info)
{
  memset(&g_machine, 0, sizeof(g_machine));

  if (!arch_initialize_machine(info, g_machine))
  {
    panic("unsupported boot machine");
  }
}

const machine_descriptor& get_machine()
{
  return g_machine;
}
