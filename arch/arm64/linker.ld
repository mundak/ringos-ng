/*
 * Phase 1 placeholder linker script for arm64.
 *
 * Loads the kernel image at the QEMU virt machine's default kernel entry
 * address. This will be refined during Phase 4 when the full arm64 boot
 * path is implemented.
 */

ENTRY(_start)

SECTIONS
{
  . = 0x40000000;

  .text : {
    *(.text .text.*)
  }

  .rodata : {
    *(.rodata .rodata.*)
  }

  .data : {
    *(.data .data.*)
  }

  .bss : {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }
}
