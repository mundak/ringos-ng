#pragma once

#include <stddef.h>
#include <stdint.h>

// This header exposes the stable ABI constants that user programs need in
// order to issue syscalls or interpret results.

// All syscall entry points return one of these signed status codes.
// `0` means success and negative values indicate failure.
enum
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

// Core syscall numbers shared by the kernel, SDK, and user programs.
enum
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
