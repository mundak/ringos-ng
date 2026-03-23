#include "boot_info.h"
#include "console.h"
#include "kernel.h"
#include "panic.h"

namespace ringos
{

  [[noreturn]] void kernel_main(const BootInfo& boot_info)
  {
    if (boot_info.arch_id == k_arch_x64)
    {
      console_write("ringos x64\n");
    }
    else if (boot_info.arch_id == k_arch_arm64)
    {
      console_write("ringos arm64\n");
    }
    else
    {
      panic("unknown architecture id");
    }

    console_write("hello world\n");

    while (true)
    {
    }
  }

} // namespace ringos
