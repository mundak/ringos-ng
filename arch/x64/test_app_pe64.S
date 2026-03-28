.text
.globl user_start

user_start:
  lea ring3_message(%rip), %rdi
  mov $1, %eax
  syscall

  xor %edi, %edi
  mov $2, %eax
  syscall

.Lhang:
  jmp .Lhang

ring3_message:
  .asciz "x64 PE64 test app reached ring3"
