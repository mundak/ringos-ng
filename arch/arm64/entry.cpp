#include "boot_info.h"
#include "kernel.h"
#include "memory.h"

extern "C" [[noreturn]] void arm64_entry()
{
  run_global_constructors();

  boot_info info {};
  info.arch_id = ARCH_ARM64;

  kernel_main(info);
}
