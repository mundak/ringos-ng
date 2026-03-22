/*
 * Phase 1 placeholder linker script for x64.
 *
 * Loads the kernel image at 1 MiB, which is the conventional entry address
 * for a Multiboot2 kernel under QEMU. This will be refined during Phase 3
 * when the full x64 boot path is implemented.
 */

ENTRY(_start)

SECTIONS
{
  . = 0x100000;

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
