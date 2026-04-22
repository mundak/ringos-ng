#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum status
{
  STATUS_OK = 0,
  STATUS_INVALID_ARGUMENT = -1,
  STATUS_BAD_HANDLE = -2,
  STATUS_WRONG_TYPE = -3,
  STATUS_BUFFER_TOO_SMALL = -4,
  STATUS_PEER_CLOSED = -5,
  STATUS_WOULD_BLOCK = -6,
  STATUS_TIMED_OUT = -7,
  STATUS_NO_MEMORY = -8,
  STATUS_FAULT = -9,
  STATUS_NOT_SUPPORTED = -10,
  STATUS_BAD_STATE = -11,
  STATUS_NOT_FOUND = -12,
};

enum syscall
{
  SYSCALL_DEBUG_LOG = 1,
  SYSCALL_THREAD_EXIT = 2,
  SYSCALL_RPC_REGISTER = 3,
  SYSCALL_RPC_OPEN = 4,
  SYSCALL_RPC_CALL = 5,
  SYSCALL_DEVICE_MEMORY_MAP = 6,
  SYSCALL_RPC_CLOSE = 7,
  SYSCALL_RPC_COMPLETE = 8,
};

#define RINGOS_HANDLE_INVALID 0

typedef uint64_t ringos_handle;

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

__attribute__((noreturn)) void ringos_thread_exit(uint64_t exit_status);

#ifdef __cplusplus
}
#endif
