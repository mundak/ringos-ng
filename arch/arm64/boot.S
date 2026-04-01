/*
 * arm64 minimal boot entry for QEMU virt direct kernel loading.
 *
 * QEMU virt loads the kernel at 0x40000000 and jumps to _start in EL1
 * with the MMU off. This code sets up a stack, clears BSS, and branches
 * to the C++ entry point.
 */

.section .text._start
.global _start

_start:
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
  /* Branch to C++ startup code. */
  bl   arm64_entry

  /* Should never return — hang if it does. */
.Lhang:
  wfe
  b    .Lhang
