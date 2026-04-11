#include "qemu_arm64_virt_machine.h"

#include "klibc/string.h"

namespace
{
  constexpr char QEMU_ARM64_VIRT_NAME[] = "qemu-arm64-virt";

  void initialize_qemu_arm64_virt_machine_descriptor(const boot_info& info, machine_descriptor& out_machine)
  {
    out_machine.arch_id = info.arch_id;
    out_machine.machine_kind = MACHINE_KIND_QEMU_ARM64_VIRT;
    copy_string(out_machine.name, sizeof(out_machine.name), QEMU_ARM64_VIRT_NAME);
  }
}

bool try_initialize_qemu_arm64_virt_machine(const boot_info& info, machine_descriptor& out_machine)
{
  if (info.arch_id != ARCH_ARM64)
  {
    return false;
  }

  initialize_qemu_arm64_virt_machine_descriptor(info, out_machine);
  return true;
}
