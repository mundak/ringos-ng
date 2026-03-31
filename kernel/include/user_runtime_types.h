#pragma once

#include "user_space.h"

#include <stddef.h>
#include <stdint.h>

static constexpr uint32_t USER_RUNTIME_MAX_PROCESSES = 4;
static constexpr uint32_t USER_RUNTIME_MAX_THREADS = 4;
static constexpr uint32_t USER_RUNTIME_MAX_CHANNELS = 8;
static constexpr uint32_t USER_RUNTIME_MAX_SHARED_MEMORY_OBJECTS = 4;
static constexpr size_t USER_RUNTIME_KERNEL_STACK_SIZE = 4096;

enum class user_thread_state : uint32_t
{
  EMPTY = 0,
  READY = 1,
  RUNNING = 2,
  EXITED = 3,
};

struct address_space
{
  uintptr_t arch_root_table;
  uintptr_t user_base;
  size_t user_size;
  uintptr_t user_host_base;
};

struct thread_context
{
  uintptr_t instruction_pointer;
  uintptr_t stack_pointer;
  uintptr_t flags;
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
  address_space address_space;
  thread_context thread_context;
};
