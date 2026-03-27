// x64 boot entry — Multiboot startup for QEMU.
//
// Receives control in 32-bit protected mode from the Multiboot loader,
// identity-maps the first 2 MiB, enables long mode, clears BSS, and
// transfers to the C++ kernel entry.

// ---------------------------------------------------------------------------
// Multiboot header — must appear within the first 8 KiB of the image.
// ---------------------------------------------------------------------------
.set MULTIBOOT_MAGIC, 0x1BADB002
.set MULTIBOOT_FLAGS, 0x00000000

.section .multiboot, "a"
.align 4

multiboot_header:
  .long MULTIBOOT_MAGIC
  .long MULTIBOOT_FLAGS
  .long -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)            // Checksum

// ---------------------------------------------------------------------------
// 32-bit startup — runs in protected mode with flat segments.
// ---------------------------------------------------------------------------
.section .text
.code32
.global _start

_start:
  cli

  // ---- Clear BSS ----
  movl $__bss_start, %edi
  movl $__bss_end, %ecx
  subl %edi, %ecx
  shrl $2, %ecx
  xorl %eax, %eax
  rep stosl

  // ---- Identity-map the first 2 MiB using 2 MiB pages ----

  // PML4[0] -> PDPT (present + writable)
  movl $boot_pdpt, %eax
  orl  $0x03, %eax
  movl %eax, (boot_pml4)

  // PDPT[0] -> PD (present + writable)
  movl $boot_pd, %eax
  orl  $0x03, %eax
  movl %eax, (boot_pdpt)

  // PD[0] -> 2 MiB huge page at physical 0 (present + writable + page-size)
  movl $0x83, (boot_pd)

  // ---- Load the boot GDT ----
  lgdt (boot_gdt_ptr)

  // ---- Enable PAE (CR4 bit 5) ----
  movl %cr4, %eax
  orl  $(1 << 5), %eax
  movl %eax, %cr4

  // ---- Point CR3 at the PML4 ----
  movl $boot_pml4, %eax
  movl %eax, %cr3

  // ---- Enable long mode (IA32_EFER.LME, bit 8) ----
  movl $0xC0000080, %ecx
  rdmsr
  orl  $(1 << 8), %eax
  wrmsr

  // ---- Enable paging (CR0.PG, bit 31) ----
  movl %cr0, %eax
  orl  $(1 << 31), %eax
  movl %eax, %cr0

  // ---- Far jump into the 64-bit code segment ----
  ljmpl $0x08, $start64

// ---------------------------------------------------------------------------
// 64-bit startup
// ---------------------------------------------------------------------------
.code64

start64:
  // Reload data segments with the 64-bit data selector
  movw $0x10, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss

  // Set up the kernel stack
  leaq boot_stack_top(%rip), %rsp

  // Call the C++ entry point (never returns)
  call x64_entry

  // Unreachable — halt if x64_entry somehow returns
.Lhalt:
  cli
  hlt
  jmp .Lhalt

// ---------------------------------------------------------------------------
// Boot GDT — minimal descriptors for the 32-to-64 transition.
// ---------------------------------------------------------------------------
.section .rodata
.align 16

boot_gdt:
  .quad 0x0000000000000000      // Null descriptor
  .quad 0x00209A0000000000      // 64-bit code: L=1, D=0, P=1, DPL=0, E=1, R=1
  .quad 0x0000920000000000      // 64-bit data: P=1, DPL=0, W=1
boot_gdt_end:

boot_gdt_ptr:
  .short boot_gdt_end - boot_gdt - 1   // Limit
  .long  boot_gdt                       // Base (32-bit address for initial lgdt)

// ---------------------------------------------------------------------------
// Page tables and boot stack — in BSS, zeroed during startup.
// ---------------------------------------------------------------------------
.section .bss

.align 4096
boot_pml4: .skip 4096
boot_pdpt: .skip 4096
boot_pd:   .skip 4096

.align 16
boot_stack_bottom: .skip 16384
boot_stack_top:
