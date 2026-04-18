#pragma once

#include "device_memory_type.h"
#include "user_space.h"

#include <stddef.h>
#include <stdint.h>

static constexpr uint32_t USER_RUNTIME_MAX_PROCESSES = 4;
static constexpr uint32_t USER_RUNTIME_MAX_THREADS = 4;
static constexpr uint32_t USER_RUNTIME_MAX_DEVICE_MEMORY_OBJECTS = 4;
static constexpr uint32_t USER_RUNTIME_MAX_RPC_ENDPOINTS = 8;
static constexpr uint32_t USER_RUNTIME_MAX_RPC_CHANNELS = 8;
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

enum user_thread_resume_kind : uint32_t
{
  USER_THREAD_RESUME_KIND_SYSCALL = 0,
  USER_THREAD_RESUME_KIND_RPC = 1,
};

enum process_guest_architecture : uint32_t
{
  PROCESS_GUEST_ARCHITECTURE_UNKNOWN = 0,
  PROCESS_GUEST_ARCHITECTURE_X64 = 1,
  PROCESS_GUEST_ARCHITECTURE_ARM64 = 2,
};

enum process_personality : uint32_t
{
  PROCESS_PERSONALITY_UNKNOWN = 0,
  PROCESS_PERSONALITY_RINGOS = 1,
  PROCESS_PERSONALITY_WINDOWS = 2,
};

enum process_execution_backend : uint32_t
{
  PROCESS_EXECUTION_BACKEND_UNKNOWN = 0,
  PROCESS_EXECUTION_BACKEND_NATIVE = 1,
  PROCESS_EXECUTION_BACKEND_X64_EMULATOR = 2,
};

struct process_metadata
{
  process_guest_architecture guest_architecture;
  process_personality personality;
  process_execution_backend execution_backend;
};

struct address_space
{
  uintptr_t arch_root_table;
  uintptr_t user_base;
  size_t user_size;
  uintptr_t user_host_base;
  device_memory_type device_memory_type;
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

struct user_thread_resume
{
  user_thread_resume_kind kind;
  uintptr_t instruction_pointer;
  uintptr_t stack_pointer;
  uintptr_t flags;
  uintptr_t argument0;
  int32_t status_code;
  uintptr_t rpc_completion_address;
};

struct user_syscall_context
{
  uint64_t syscall_number;
  uintptr_t trap_instruction_pointer;
  uintptr_t stack_pointer;
  uintptr_t trap_flags;
  uint64_t argument0;
  uint64_t argument1;
  uint64_t argument2;
  uint64_t argument3;
  uint64_t argument4;
  uint64_t argument5;
};

struct initial_process_configuration
{
  process_metadata metadata;
  address_space address_space;
  thread_context thread_context;
};

user_syscall_context make_user_syscall_context(
  uint64_t syscall_number,
  uintptr_t trap_instruction_pointer,
  uintptr_t stack_pointer,
  uintptr_t trap_flags,
  uint64_t argument0,
  uint64_t argument1,
  uint64_t argument2,
  uint64_t argument3,
  uint64_t argument4,
  uint64_t argument5);

thread_context make_thread_context_from_syscall(const user_syscall_context& syscall_context);

struct initial_user_runtime_bootstrap
{
  uint32_t process_count;
  uint32_t initial_process_index;
  initial_process_configuration initial_processes[USER_RUNTIME_MAX_INITIAL_PROCESSES];
};
