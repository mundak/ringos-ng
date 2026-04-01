#pragma once

#include "user_space.h"

#include <stddef.h>
#include <stdint.h>

static constexpr uint32_t USER_RUNTIME_MAX_PROCESSES = 4;
static constexpr uint32_t USER_RUNTIME_MAX_THREADS = 4;
static constexpr uint32_t USER_RUNTIME_MAX_CHANNELS = 8;
static constexpr uint32_t USER_RUNTIME_MAX_DEVICE_MEMORY_OBJECTS = 4;
static constexpr uint32_t USER_RUNTIME_MAX_SHARED_MEMORY_OBJECTS = 4;
static constexpr uint32_t USER_RUNTIME_MAX_INITIAL_PROCESSES = 2;
static constexpr size_t USER_RUNTIME_KERNEL_STACK_SIZE = 4096;
static constexpr uint32_t USER_THREAD_ARCH_PRESERVED_REGISTER_COUNT = 12;
static constexpr uint32_t USER_THREAD_ARCH_PRESERVED_SIMD_QWORD_COUNT = 20;

enum user_thread_state : uint32_t
{
  USER_THREAD_STATE_EMPTY = 0,
  USER_THREAD_STATE_READY = 1,
  USER_THREAD_STATE_RUNNING = 2,
  USER_THREAD_STATE_BLOCKED = 3,
  USER_THREAD_STATE_EXITED = 4,
};

struct address_space
{
  uintptr_t arch_root_table;
  uintptr_t user_base;
  size_t user_size;
  uintptr_t user_host_base;
  uintptr_t rpc_transfer_user_address;
  uintptr_t rpc_transfer_host_address;
  size_t rpc_transfer_size;
  uintptr_t device_memory_user_address;
  uintptr_t device_memory_host_address;
  size_t device_memory_size;
};

struct thread_context
{
  uintptr_t instruction_pointer;
  uintptr_t stack_pointer;
  uintptr_t flags;
  uintptr_t argument0;
};

struct user_syscall_context
{
  uint64_t syscall_number;
  uint64_t argument0;
  uint64_t argument1;
  uint64_t argument2;
  uint64_t argument3;
  uintptr_t stack_pointer;
};

struct initial_user_runtime_bootstrap
{
  uint32_t process_count;
  uint32_t initial_process_index;
  address_space address_space[USER_RUNTIME_MAX_INITIAL_PROCESSES];
  thread_context thread_context[USER_RUNTIME_MAX_INITIAL_PROCESSES];
};
