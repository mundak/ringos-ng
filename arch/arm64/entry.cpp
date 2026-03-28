#include "boot_info.h"
#include "kernel.h"

extern "C" [[noreturn]] void arm64_entry()
{
  boot_info info {};
  info.arch_id = ARCH_ARM64;

  kernel_main(info);
}
