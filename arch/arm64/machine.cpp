#include "arch_machine.h"
#include "klibc/memory.h"
#include "machines/qemu_arm64_virt_machine.h"

bool arch_initialize_machine(const boot_info& info, machine_descriptor& out_machine)
{
  memset(&out_machine, 0, sizeof(out_machine));

  if (info.arch_id != ARCH_ARM64)
  {
    return false;
  }

  out_machine.arch_id = info.arch_id;
  return try_initialize_qemu_arm64_virt_machine(info, out_machine);
}
