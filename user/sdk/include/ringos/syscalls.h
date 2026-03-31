#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RINGOS_SYSCALL_DEBUG_LOG ((uint64_t) 1)
#define RINGOS_SYSCALL_THREAD_EXIT ((uint64_t) 2)
#define RINGOS_SYSCALL_RPC_CALL ((uint64_t) 3)

int32_t ringos_syscall0(uint64_t syscall_number);
int32_t ringos_syscall1(uint64_t syscall_number, uintptr_t argument0);
int32_t ringos_syscall2(uint64_t syscall_number, uintptr_t argument0, uintptr_t argument1);
int32_t ringos_syscall3(uint64_t syscall_number, uintptr_t argument0, uintptr_t argument1, uintptr_t argument2);
int32_t ringos_syscall4(
  uint64_t syscall_number, uintptr_t argument0, uintptr_t argument1, uintptr_t argument2, uintptr_t argument3);
int32_t ringos_syscall5(
  uint64_t syscall_number,
  uintptr_t argument0,
  uintptr_t argument1,
  uintptr_t argument2,
  uintptr_t argument3,
  uintptr_t argument4);
int32_t ringos_syscall6(
  uint64_t syscall_number,
  uintptr_t argument0,
  uintptr_t argument1,
  uintptr_t argument2,
  uintptr_t argument3,
  uintptr_t argument4,
  uintptr_t argument5);

#ifdef __cplusplus
}
#endif
