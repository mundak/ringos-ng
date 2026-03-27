#include "boot_info.h"
#include "kernel.h"
#include "pl011.h"

extern "C" [[noreturn]] void arm64_entry()
{
  pl011_init();

  boot_info info {};
  info.m_arch_id = ARCH_ARM64;

  kernel_main(info);
}
