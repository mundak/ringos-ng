#pragma once

#include <stddef.h>
#include <stdint.h>

#include "channel.h"
#include "kernel_object.h"
#include "process.h"
#include "shared_memory_object.h"
#include "thread.h"
#include "user_runtime_types.h"

struct initial_user_runtime_platform
{
  void* context;
  void (*initialize)(
    void* context,
    initial_user_runtime_bootstrap& bootstrap);
  void (*prepare_thread_launch)(
    void* context,
    const process& initial_process,
    const thread& initial_thread);
  void (*enter_user_thread)(
    void* context,
    const process& initial_process,
    const thread& initial_thread);
};

class user_runtime final
{
public:
  void reset();
  process* create_process(const address_space& address_space_info);
  thread* create_thread(
    process& process_context,
    const thread_context& initial_context,
    uint64_t* out_thread_handle);
  bool create_channel_pair(
    uint64_t* out_first_handle,
    uint64_t* out_second_handle);
  shared_memory_object* create_shared_memory_object(
    uintptr_t user_address,
    size_t size,
    uint64_t* out_handle);
  bool validate_user_range(
    const process& owner_process,
    uintptr_t user_address,
    size_t length) const;
  int32_t copy_user_string(
    const thread& owner_thread,
    uintptr_t user_address,
    char* buffer,
    size_t buffer_size) const;
  thread* current_thread();
  void set_current_thread(thread* current_thread);
  int32_t dispatch_syscall(
    uint64_t syscall_number,
    uint64_t argument0);
  bool should_resume_current_thread() const;

private:
  uint64_t allocate_handle_value();
  void grant_process_access(kernel_object& object);

  process m_processes[USER_RUNTIME_MAX_PROCESSES];
  thread m_threads[USER_RUNTIME_MAX_THREADS];
  channel m_channels[USER_RUNTIME_MAX_CHANNELS];
  shared_memory_object m_shared_memory_objects[USER_RUNTIME_MAX_SHARED_MEMORY_OBJECTS];
  thread* m_current_thread;
  uint32_t m_next_process_id;
  uint32_t m_next_thread_id;
  uint32_t m_next_channel_id;
  uint32_t m_next_shared_memory_id;
  uint64_t m_next_handle_value;
};

user_runtime& kernel_user_runtime();
[[noreturn]] void run_initial_user_runtime(initial_user_runtime_platform& platform);