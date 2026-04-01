.text.globl user_start

  user_start : sub $40,
  % rsp lea ring3_message(% rip), % rcx call* __imp_OutputDebugStringA(% rip)

                                    xor
  % ecx,
  % ecx call* __imp_ExitProcess(% rip)

      .Lhang : jmp
      .Lhang

      .section.rdata,
  "dr" ring3_message
  :.asciz "x64 PE64 Win32 import test app reached ring3"

     .section.idata$2,
  "dr" import_descriptors :.long import_lookup_table.long 0 .long 0 .long kernel32_name
                             .long import_address_table

                           null_import_descriptor :.long 0 .long 0 .long 0 .long 0
                             .long 0

                             .section.idata$4,
  "dr" import_lookup_table :.quad hint_name_OutputDebugStringA.quad hint_name_ExitProcess
                              .quad 0

                              .section.idata$5,
  "drw" import_address_table : __imp_OutputDebugStringA :.quad hint_name_OutputDebugStringA __imp_ExitProcess
  :.quad hint_name_ExitProcess
     .quad 0

     .section.idata$6,
  "dr" hint_name_OutputDebugStringA :.short 0 .asciz "OutputDebugStringA"
                                       .p2align 1

                                     hint_name_ExitProcess
  :.short 0 .asciz "ExitProcess".p2align 1

                                       .section.idata$7,
  "dr" kernel32_name :.asciz "KERNEL32.dll"
