.section .text
.code64

.global x64_enter_user_thread
.global x64_syscall_entry
.global x64_user_thread_exit
.extern x64_handle_syscall

x64_enter_user_thread:
  movw $0x23, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  pushq $0x23
  pushq %rsi
  pushq %rdx
  pushq $0x2B
  pushq %rdi
  iretq

x64_syscall_entry:
  swapgs
  movq %rsp, %gs:0
  movq %gs:8, %rsp

  subq $160, %rsp
  movdqu %xmm6, 0(%rsp)
  movdqu %xmm7, 16(%rsp)
  movdqu %xmm8, 32(%rsp)
  movdqu %xmm9, 48(%rsp)
  movdqu %xmm10, 64(%rsp)
  movdqu %xmm11, 80(%rsp)
  movdqu %xmm12, 96(%rsp)
  movdqu %xmm13, 112(%rsp)
  movdqu %xmm14, 128(%rsp)
  movdqu %xmm15, 144(%rsp)

  pushq %rbx
  pushq %rbp
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15

  pushq %gs:0
  pushq %r11
  pushq %rcx
  pushq %rax
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %r10
  pushq %r8
  pushq %r9

  movq %rsp, %rdi
  call x64_handle_syscall
  testb %al, %al
  jz .Lthread_exit

  movq 0(%rsp), %r9
  movq 8(%rsp), %r8
  movq 16(%rsp), %r10
  movq 24(%rsp), %rdx
  movq 32(%rsp), %rsi
  movq 40(%rsp), %rdi
  movq 48(%rsp), %rax
  movq 56(%rsp), %rcx
  movq 64(%rsp), %r11
  movq 80(%rsp), %r15
  movq 88(%rsp), %r14
  movq 96(%rsp), %r13
  movq 104(%rsp), %r12
  movq 112(%rsp), %rbp
  movq 120(%rsp), %rbx
  movdqu 128(%rsp), %xmm6
  movdqu 144(%rsp), %xmm7
  movdqu 160(%rsp), %xmm8
  movdqu 176(%rsp), %xmm9
  movdqu 192(%rsp), %xmm10
  movdqu 208(%rsp), %xmm11
  movdqu 224(%rsp), %xmm12
  movdqu 240(%rsp), %xmm13
  movdqu 256(%rsp), %xmm14
  movdqu 272(%rsp), %xmm15
  movq 72(%rsp), %rsp
  swapgs
  sysretq

.Lthread_exit:
  addq $288, %rsp
  call x64_user_thread_exit
