/*
 * arm64 linker script for QEMU virt direct kernel loading.
 *
 * QEMU loads the kernel image at 0x40000000 and branches to _start.
 * The layout provides a text section, read-only data, initialized data,
 * BSS, and a 16 KiB boot stack.
 */

ENTRY(_start)

SECTIONS
{
  . = 0x40000000;

  .text : ALIGN(4096)
  {
    KEEP(*(.text._start))
    *(.text .text.*)
  }

  .rodata : ALIGN(4096)
  {
    *(.rodata .rodata.*)
  }

  .data : ALIGN(4096)
  {
    *(.data .data.*)
  }

  .bss : ALIGN(4096)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
    __bss_end = .;
  }

  /* 16 KiB boot stack — grows downward from __stack_top. */
  .stack (NOLOAD) : ALIGN(16)
  {
    __stack_bottom = .;
    . += 16384;
    __stack_top = .;
  }

  /DISCARD/ :
  {
    *(.comment)
    *(.note .note.*)
    *(.eh_frame*)
  }
}
