#pragma once

#include <stddef.h>
#include <stdint.h>

// This header exposes the stable ABI constants that user programs need in
// order to issue syscalls or interpret results.

// All syscall entry points return one of these signed 32-bit status codes.
// `0` means success and negative values indicate failure.
static constexpr int32_t STATUS_OK = 0;
static constexpr int32_t STATUS_INVALID_ARGUMENT = -1;
static constexpr int32_t STATUS_BAD_HANDLE = -2;
static constexpr int32_t STATUS_WRONG_TYPE = -3;
static constexpr int32_t STATUS_BUFFER_TOO_SMALL = -4;
static constexpr int32_t STATUS_PEER_CLOSED = -5;
static constexpr int32_t STATUS_WOULD_BLOCK = -6;
static constexpr int32_t STATUS_TIMED_OUT = -7;
static constexpr int32_t STATUS_NO_MEMORY = -8;
static constexpr int32_t STATUS_FAULT = -9;
static constexpr int32_t STATUS_NOT_SUPPORTED = -10;
static constexpr int32_t STATUS_BAD_STATE = -11;
static constexpr int32_t STATUS_NOT_FOUND = -12;

// Core syscall numbers shared by the kernel, SDK, and user programs.
static constexpr uint64_t SYSCALL_DEBUG_LOG = 1;
static constexpr uint64_t SYSCALL_THREAD_EXIT = 2;
static constexpr uint64_t SYSCALL_RPC_CALL = 3;
static constexpr uint64_t SYSCALL_RPC_WAIT = 4;
static constexpr uint64_t SYSCALL_RPC_REPLY = 5;
static constexpr uint64_t SYSCALL_DEVICE_MEMORY_MAP = 6;
static constexpr uint64_t SYSCALL_RPC_OPEN = 7;
static constexpr uint64_t SYSCALL_CONSOLE_QUERY = 8;

// Windows compatibility syscalls live in a separate range for imported x64 PE
// entry points.
static constexpr uint64_t SYSCALL_WINDOWS_GET_STD_HANDLE = 0x100;
static constexpr uint64_t SYSCALL_WINDOWS_WRITE_FILE = 0x101;
static constexpr uint64_t SYSCALL_WINDOWS_EXIT_PROCESS = 0x102;
