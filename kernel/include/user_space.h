#pragma once

#include <stddef.h>
#include <stdint.h>

// Stage 1 is the first kernel bring-up step that establishes a safe trap
// boundary between user mode and kernel mode. The SDK-facing surface in this
// header is intentionally small: only ABI constants that user programs need in
// order to issue syscalls or interpret results belong here.

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

// Stage 1 proof-path syscall numbers. These are temporary validation hooks for
// the first user-mode bring-up, not the final SDK shape.
static constexpr uint64_t STAGE1_SYSCALL_DEBUG_LOG = 1;
static constexpr uint64_t STAGE1_SYSCALL_THREAD_EXIT = 2;

// Stage 2B widens the proof path just enough to emulate a tiny Windows console
// surface for imported x64 PE entry points.
static constexpr uint64_t STAGE2_SYSCALL_WINDOWS_GET_STD_HANDLE = 0x100;
static constexpr uint64_t STAGE2_SYSCALL_WINDOWS_WRITE_FILE = 0x101;
static constexpr uint64_t STAGE2_SYSCALL_WINDOWS_EXIT_PROCESS = 0x102;

