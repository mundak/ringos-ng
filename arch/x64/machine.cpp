#include "arch_machine.h"
#include "klibc/memory.h"
#include "klibc/string.h"

bool arch_initialize_machine(const boot_info& info, machine_descriptor& out_machine)
{
  memset(&out_machine, 0, sizeof(out_machine));
  out_machine.arch_id = info.arch_id;
  out_machine.machine_kind = MACHINE_KIND_X64_GENERIC;
  copy_string(out_machine.name, sizeof(out_machine.name), "x64-generic");
  return info.arch_id == ARCH_X64;
}
