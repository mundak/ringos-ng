#include "boot_info.h"
#include "kernel.h"

extern "C" [[noreturn]] void x64_entry()
{
  boot_info info;
  info.arch_id = ARCH_X64;

  kernel_main(info);
}
