.section.text
  .code64

  .global x64_enter_user_thread.global x64_syscall_entry.global x64_user_thread_exit.extern x64_handle_syscall

    x64_enter_user_thread : movw $0x23,
  % ax movw % ax, % ds movw % ax, % es movw % ax, % fs movw % ax,
  % gs pushq $0x23 pushq % rsi pushq % rdx pushq $0x2B pushq
  % rdi iretq

    x64_syscall_entry : swapgs movq
                        % rsp,
  % gs : 0 movq % gs : 8, % rsp

                            pushq
  % gs : 0 pushq % r11 pushq % rcx pushq % rax pushq % rdi pushq % rsi pushq % rdx pushq % r10 pushq % r8 pushq
  % r9

    movq
  % rsp,
  % rdi call x64_handle_syscall testb % al, % al jz.Lthread_exit

                                                movq 0(% rsp),
  % r9 movq 8(% rsp), % r8 movq 16(% rsp), % r10 movq 24(% rsp), % rdx movq 32(% rsp), % rsi movq 40(% rsp),
  % rdi movq 48(% rsp), % rax movq 56(% rsp), % rcx movq 64(% rsp), % r11 movq 72(% rsp), % rsp swapgs sysretq

                                                                                              .Lthread_exit : addq $80,
  % rsp call x64_user_thread_exit
