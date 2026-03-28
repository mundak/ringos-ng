#pragma once

#include "user_space.h"

static constexpr uint32_t STAGE1_MAX_PROCESSES = 4;
static constexpr uint32_t STAGE1_MAX_THREADS = 4;
static constexpr uint32_t STAGE1_MAX_CHANNELS = 8;
static constexpr uint32_t STAGE1_MAX_SHARED_MEMORY_OBJECTS = 4;
static constexpr uint32_t STAGE1_MAX_HANDLES = 16;
static constexpr size_t STAGE1_KERNEL_STACK_SIZE = 4096;

enum class object_type : uint32_t
{
  none = 0,
  process = 1,
  thread = 2,
  channel = 3,
  shared_memory = 4,
};

enum class thread_state : uint32_t
{
  empty = 0,
  ready = 1,
  running = 2,
  exited = 3,
};

struct address_space
{
  uintptr_t m_arch_root_table;
  uintptr_t m_user_base;
  size_t m_user_size;
};

struct process;
struct channel;
struct shared_memory_object;

struct handle_entry
{
  bool m_in_use;
  uint8_t m_padding[7];
  uint64_t m_handle;
  object_type m_object_type;
  void* m_object;
};

struct process
{
  bool m_in_use;
  uint8_t m_padding[7];
  uint32_t m_id;
  uint32_t m_reserved;
  address_space m_address_space;
  handle_entry m_handle_table[STAGE1_MAX_HANDLES];
};

struct thread_context
{
  uintptr_t m_instruction_pointer;
  uintptr_t m_stack_pointer;
  uintptr_t m_flags;
};

struct thread
{
  bool m_in_use;
  uint8_t m_padding[3];
  thread_state m_state;
  uint32_t m_id;
  uint32_t m_reserved;
  process* m_process;
  thread_context m_user_context;
  uint64_t m_exit_status;
  alignas(16) uint8_t m_kernel_stack[STAGE1_KERNEL_STACK_SIZE];
};

struct channel
{
  bool m_in_use;
  uint8_t m_padding[7];
  uint32_t m_id;
  uint32_t m_reserved;
  process* m_owner_process;
  channel* m_peer;
};

struct shared_memory_object
{
  bool m_in_use;
  uint8_t m_padding[7];
  uint32_t m_id;
  uint32_t m_reserved;
  process* m_owner_process;
  uintptr_t m_user_address;
  size_t m_size;
};

void stage1_reset_kernel_state();
process* stage1_create_process(const address_space& address_space_info);
thread* stage1_create_thread(
  process& owner_process, const thread_context& initial_context, uint64_t* out_thread_handle);
bool stage1_create_channel_pair(process& owner_process, uint64_t* out_first_handle, uint64_t* out_second_handle);
shared_memory_object* stage1_create_shared_memory_object(
  process& owner_process, uintptr_t user_address, size_t size, uint64_t* out_handle);
bool stage1_validate_user_range(const process& owner_process, uintptr_t user_address, size_t length);
int32_t stage1_copy_user_string(const thread& owner_thread, uintptr_t user_address, char* buffer, size_t buffer_size);
thread* stage1_get_current_thread();
void stage1_set_current_thread(thread* current_thread);
int32_t stage1_dispatch_syscall(uint64_t syscall_number, uint64_t argument0);
bool stage1_should_resume_current_thread();