/*
 * x64 linker script — Multiboot-compliant layout for QEMU.
 *
 * The image is loaded at 1 MiB, the conventional Multiboot entry address.
 * The Multiboot header must reside within the first 8 KiB of the image.
 */

ENTRY(_start)

SECTIONS
{
  . = 0x100000;

  /* Multiboot header — must be within the first 8 KiB */
  .multiboot : ALIGN(4) {
    *(.multiboot)
  }

  .text : ALIGN(16) {
    *(.text .text.*)
  }

  .rodata : ALIGN(16) {
    *(.rodata .rodata.*)
  }

  .init_array : ALIGN(16) {
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  }

  .data : ALIGN(16) {
    *(.data .data.*)
  }

  .bss : ALIGN(4096) {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }

  /DISCARD/ : {
    *(.comment)
    *(.note*)
    *(.eh_frame*)
  }
}
