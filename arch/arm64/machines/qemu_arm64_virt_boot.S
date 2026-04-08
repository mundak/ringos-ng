/*
 * qemu-arm64-virt boot entry.
 *
 * QEMU virt loads the kernel at 0x40000000 and jumps to _start in EL1
 * with the MMU off. It passes the device-tree blob address in x0 when
 * the ARM64 Linux Image header magic is present; without this header QEMU
 * does not set x0 to the DTB address.
 *
 * The first 64 bytes form the ARM64 Linux Image header so that QEMU
 * follows the Linux boot protocol (x0 = DTB address, x1=x2=x3=0).
 * code0 is a branch over the rest of the header to the real boot code.
 */

.section .text._start
.global _start

_start:
  /* ARM64 Linux Image header — 64 bytes.
   * QEMU checks the magic at offset 56 (0x644d5241) to decide whether to
   * follow the Linux AArch64 boot protocol and hand the DTB address in x0. */
  b     _boot           /* code0: branch to actual boot code */
  .long 0               /* code1 */
  .quad 0               /* text_offset: 0 = load at start of RAM */
  .quad 0               /* image_size: 0 = unspecified */
  .quad 0               /* flags */
  .quad 0               /* res2 */
  .quad 0               /* res3 */
  .quad 0               /* res4 */
  .long 0x644d5241      /* magic: "ARM\x64" little-endian */
  .long 0               /* res5 */

_boot:
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
