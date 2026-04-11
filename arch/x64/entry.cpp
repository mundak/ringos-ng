#include "boot_info.h"
#include "kernel.h"
#include "klibc/memory.h"

extern "C" [[noreturn]] void x64_entry()
{
  run_global_constructors();

  boot_info info {};
  info.arch_id = ARCH_X64;

  kernel_main(info);
}
