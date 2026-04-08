#include "arch_machine.h"

#include "memory.h"

namespace
{
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
}

bool arch_initialize_machine(const boot_info& info, machine_descriptor& out_machine)
{
  memset(&out_machine, 0, sizeof(out_machine));
  out_machine.arch_id = info.arch_id;
  out_machine.machine_kind = MACHINE_KIND_X64_GENERIC;
  copy_string(out_machine.name, sizeof(out_machine.name), "x64-generic");
  return info.arch_id == ARCH_X64;
}
