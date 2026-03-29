.text
.globl user_start

user_start:
  mov $-11, %ecx
  push %rcx
  pop %rdi
  xor %eax, %eax
  push %rax
  push %rax
  push %rax
  push %rax
  push %rax
  call *__imp_GetStdHandle(%rip)
  add $40, %rsp

  lea ring3_message(%rip), %rdx
  push %rax
  pop %rcx
  push %rax
  pop %rdi
  mov $(ring3_message_end - ring3_message - 1), %eax
  push %rax
  pop %r8
  lea ring3_bytes_written(%rip), %rax
  push %rax
  pop %r9
  xor %eax, %eax
  push %rax
  push %rax
  push %rax
  push %rax
  push %rax
  call *__imp_WriteFile(%rip)
  add $40, %rsp

  xor %ecx, %ecx
  xor %edi, %edi
  xor %eax, %eax
  push %rax
  push %rax
  push %rax
  push %rax
  push %rax
  call *__imp_ExitProcess(%rip)

.Lhang:
  jmp .Lhang

.section .rdata,"dr"
ring3_message:
  .asciz "x64 PE64 test app reached ring3"
ring3_message_end:

.section .data,"dw"
ring3_bytes_written:
  .long 0

.section .idata$2,"dr"
  .rva import_lookup_table
  .long 0
  .long 0
  .rva kernel32_dll_name
  .rva import_address_table
  .long 0
  .long 0
  .long 0
  .long 0

.section .idata$4,"dr"
  .balign 8
import_lookup_table:
  .rva hint_name_get_std_handle
  .long 0
  .rva hint_name_write_file
  .long 0
  .rva hint_name_exit_process
  .long 0
  .long 0
  .long 0

.section .idata$5,"drw"
  .balign 8
  .globl __imp_GetStdHandle
  .globl __imp_WriteFile
  .globl __imp_ExitProcess
import_address_table:
__imp_GetStdHandle:
  .rva hint_name_get_std_handle
  .long 0
__imp_WriteFile:
  .rva hint_name_write_file
  .long 0
__imp_ExitProcess:
  .rva hint_name_exit_process
  .long 0
  .long 0
  .long 0

.section .idata$6,"dr"
hint_name_get_std_handle:
  .short 0
  .asciz "GetStdHandle"
  .balign 2
hint_name_write_file:
  .short 0
  .asciz "WriteFile"
  .balign 2
hint_name_exit_process:
  .short 0
  .asciz "ExitProcess"
  .balign 2

.section .idata$7,"dr"
kernel32_dll_name:
  .asciz "KERNEL32.dll"
