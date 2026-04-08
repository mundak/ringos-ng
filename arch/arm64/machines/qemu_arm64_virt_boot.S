/*
 * qemu-arm64-virt boot entry.
 *
 * QEMU virt loads the kernel at 0x40000000 and jumps to _start in EL1
 * with the MMU off. It passes the device-tree blob address in x0.
 * This machine-specific entry preserves the DTB pointer, sets up a stack,
 * clears BSS, and branches to the shared arm64 C++ entry point.
 */

.section .text._start
.global _start

_start:
  /* Preserve the DTB address passed by QEMU virt in x0. */
  mov  x20, x0

  /* Set up the stack pointer — grows downward from the top of the
   * reserved stack region defined in the linker script. */
  adrp x0, __stack_top
  add  x0, x0, :lo12:__stack_top
  mov  sp, x0

  /* Clear the BSS section. */
  adrp x0, __bss_start
  add  x0, x0, :lo12:__bss_start
  adrp x1, __bss_end
  add  x1, x1, :lo12:__bss_end

.Lbss_loop:
  cmp  x0, x1
  b.ge .Lbss_done
  str  xzr, [x0], #8
  b    .Lbss_loop

.Lbss_done:
  /* Branch to shared arm64 startup code with the DTB address. */
  mov  x0, x20
  bl   arm64_entry

  /* Should never return — hang if it does. */
.Lhang:
  wfe
  b    .Lhang
