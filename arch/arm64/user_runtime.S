.section .text
.align 2

.global arm64_enter_user_thread
.global arm64_exception_vectors
.global arm64_user_stub_start
.global arm64_user_stub_end
.global arm64_user_thread_exit
.extern arm64_handle_syscall
.extern arm64_unhandled_exception

.macro vector_entry label
  b \label
  .space 124
.endm

arm64_enter_user_thread:
  msr sp_el0, x1
  msr elr_el1, x0
  msr spsr_el1, x2
  eret

.align 11
arm64_exception_vectors:
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_lower_el_aarch64_sync
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception
  vector_entry arm64_unhandled_exception

arm64_lower_el_aarch64_sync:
  sub sp, sp, #208
  stp x0, x1, [sp, #0]
  stp x2, x3, [sp, #16]
  stp x4, x5, [sp, #32]
  stp x6, x7, [sp, #48]
  str x8, [sp, #64]
  mrs x9, elr_el1
  str x9, [sp, #72]
  mrs x10, spsr_el1
  str x10, [sp, #80]
  mrs x11, sp_el0
  str x11, [sp, #88]
  mrs x12, esr_el1
  str x12, [sp, #96]
  stp x19, x20, [sp, #104]
  stp x21, x22, [sp, #120]
  stp x23, x24, [sp, #136]
  stp x25, x26, [sp, #152]
  stp x27, x28, [sp, #168]
  stp x29, x30, [sp, #184]

  mov x0, sp
  bl arm64_handle_syscall
  cbz w0, .Lthread_exit

  ldr x11, [sp, #88]
  msr sp_el0, x11
  ldr x9, [sp, #72]
  msr elr_el1, x9
  ldr x10, [sp, #80]
  msr spsr_el1, x10
  ldp x0, x1, [sp, #0]
  ldp x2, x3, [sp, #16]
  ldp x4, x5, [sp, #32]
  ldp x6, x7, [sp, #48]
  ldr x8, [sp, #64]
  ldp x19, x20, [sp, #104]
  ldp x21, x22, [sp, #120]
  ldp x23, x24, [sp, #136]
  ldp x25, x26, [sp, #152]
  ldp x27, x28, [sp, #168]
  ldp x29, x30, [sp, #184]
  add sp, sp, #208
  eret

.Lthread_exit:
  add sp, sp, #208
  bl arm64_user_thread_exit

arm64_user_stub_start:
  adr x0, arm64_user_message
  mov x8, #1
  svc #0

  mov x0, #0
  mov x8, #2
  svc #0

.Lhang:
  wfe
  b .Lhang

arm64_user_message:
  .asciz "arm64 user runtime reached el0"

arm64_user_stub_end:
