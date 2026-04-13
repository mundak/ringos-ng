.section .text
.code64

.global ringos_syscall0
.global ringos_syscall1
.global ringos_syscall2
.global ringos_syscall3
.global ringos_syscall4
.global ringos_syscall5
.global ringos_syscall6
.global ringos_rpc_complete_trampoline

ringos_syscall0:
  movq %rdi, %rax
  syscall
  ret

ringos_syscall1:
  pushq %rdi
  movq %rdi, %rax
  movq %rsi, %rdi
  syscall
  popq %rdi
  ret

ringos_syscall2:
  pushq %rdi
  pushq %rsi
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  syscall
  popq %rsi
  popq %rdi
  ret

ringos_syscall3:
  pushq %rdi
  pushq %rsi
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  movq %rcx, %rdx
  syscall
  popq %rsi
  popq %rdi
  ret

ringos_syscall4:
  pushq %rdi
  pushq %rsi
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  movq %rcx, %rdx
  movq %r8, %r10
  syscall
  popq %rsi
  popq %rdi
  ret

ringos_syscall5:
  pushq %rdi
  pushq %rsi
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  movq %rcx, %rdx
  movq %r8, %r10
  movq %r9, %r8
  syscall
  popq %rsi
  popq %rdi
  ret

ringos_syscall6:
  pushq %rdi
  pushq %rsi
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  movq %rcx, %rdx
  movq %r8, %r10
  movq %r9, %r8
  movq 24(%rsp), %r9
  syscall
  popq %rsi
  popq %rdi
  ret

ringos_rpc_complete_trampoline:
  movslq %eax, %rdi
  movq $8, %rax
  syscall

.Lringos_rpc_complete_trampoline_hang:
  jmp .Lringos_rpc_complete_trampoline_hang

