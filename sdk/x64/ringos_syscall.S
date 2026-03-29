.section.text
    .code64

    .global ringos_syscall0.global ringos_syscall1.global ringos_syscall2.global ringos_syscall3.global ringos_syscall4
    .global ringos_syscall5.global ringos_syscall6

      ringos_syscall0 : movq
  % rcx,
  % rax syscall ret

    ringos_syscall1 : pushq
                      % rdi movq
                      % rcx,
  % rax movq % rdx, % rdi syscall popq
  % rdi ret

    ringos_syscall2 : pushq
                      % rdi pushq
                      % rsi movq
                      % rcx,
  % rax movq % rdx, % rdi movq % r8, % rsi syscall popq % rsi popq
  % rdi ret

    ringos_syscall3 : pushq
                      % rdi pushq
                      % rsi movq
                      % rcx,
  % rax movq % rdx, % rdi movq % r8, % rsi movq % r9, % rdx syscall popq % rsi popq
  % rdi ret

    ringos_syscall4 : pushq
                      % rdi pushq
                      % rsi movq
                      % rcx,
  % rax movq % rdx, % rdi movq % r8, % rsi movq % r9, % rdx movq 56(% rsp), % r10 syscall popq % rsi popq
  % rdi ret

  ringos_syscall5 : pushq
                    % rdi pushq
                    % rsi movq
                    % rcx,
  % rax movq % rdx, % rdi movq % r8, % rsi movq % r9, % rdx movq 56(% rsp), % r10 movq 64(% rsp),
  % r8 syscall popq % rsi popq
  % rdi ret

  ringos_syscall6 : pushq
                    % rdi pushq
                    % rsi movq
                    % rcx,
  % rax movq % rdx, % rdi movq % r8, % rsi movq % r9, % rdx movq 56(% rsp), % r10 movq 64(% rsp), % r8 movq 72(% rsp),
  % r9 syscall popq % rsi popq % rdi ret
