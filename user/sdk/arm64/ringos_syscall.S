.text
  .align 2

  .global ringos_syscall0.global ringos_syscall1.global ringos_syscall2.global ringos_syscall3.global ringos_syscall4
  .global ringos_syscall5.global ringos_syscall6

    ringos_syscall0 : mov x8,
  x0 svc #0 ret

    ringos_syscall1 : mov x8,
                      x0 mov x0,
                      x1 svc #0 ret

                        ringos_syscall2 : mov x8,
                                          x0 mov x0,
                                          x1 mov x1,
                                          x2 svc #0 ret

                                            ringos_syscall3 : mov x8,
                                                              x0 mov x0,
                                                              x1 mov x1,
                                                              x2 mov x2,
                                                              x3 svc #0 ret

                                                                ringos_syscall4 : mov x8,
                                                                                  x0 mov x0,
                                                                                  x1 mov x1,
                                                                                  x2 mov x2,
                                                                                  x3 mov x3,
                                                                                  x4 svc #0 ret

                                                                                    ringos_syscall5 : mov x8,
                                                                                                      x0 mov x0,
                                                                                                      x1 mov x1,
                                                                                                      x2 mov x2,
                                                                                                      x3 mov x3,
                                                                                                      x4 mov x4,
                                                                                                      x5 svc #0 ret

                                                                                                        ringos_syscall6
  : mov x8,
    x0 mov x0,
    x1 mov x1,
    x2 mov x2,
    x3 mov x3,
    x4 mov x4,
    x5 mov x5,
    x6 svc #0 ret
