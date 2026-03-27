#include "boot_info.h"
#include "kernel.h"
#include "serial.h"

extern "C" [[noreturn]] void x64_entry()
{
  serial_init();

  boot_info info;
  info.m_arch_id = ARCH_X64;

  kernel_main(info);
}
