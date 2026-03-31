#pragma once

#include <ringos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
enum ringos_syscall : uint64_t
#else
enum ringos_syscall
#endif
{
  RINGOS_SYSCALL_DEBUG_LOG = 1,
  RINGOS_SYSCALL_THREAD_EXIT = 2,
  RINGOS_SYSCALL_RPC_CALL = 3,
};

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

