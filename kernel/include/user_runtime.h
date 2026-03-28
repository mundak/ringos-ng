#pragma once

#include "channel.h"
#include "handle.h"
#include "kernel_object.h"
#include "kernel_object_pool.h"
#include "process.h"
#include "shared_memory_object.h"
#include "thread.h"
#include "user_runtime_types.h"

#include <stddef.h>
#include <stdint.h>

struct initial_user_runtime_platform
{
  void* context;
  void (*initialize)(void* context, initial_user_runtime_bootstrap& bootstrap);
  void (*prepare_thread_launch)(void* context, const process& initial_process, const thread& initial_thread);
  void (*enter_user_thread)(void* context, const process& initial_process, const thread& initial_thread);
};

class user_runtime final
{
public:
  void reset();
  process* create_process(const address_space& address_space_info);
  thread* create_thread(process& process_context, const thread_context& initial_context, handle_t* out_thread_handle);
  bool create_channel_pair(handle_t* out_first_handle, handle_t* out_second_handle);
  shared_memory_object* create_shared_memory_object(uintptr_t user_address, size_t size, handle_t* out_handle);
  bool validate_user_range(const process& owner_process, uintptr_t user_address, size_t length) const;
  int32_t copy_user_string(const thread& owner_thread, uintptr_t user_address, char* buffer, size_t buffer_size) const;
  thread* get_current_thread();
  void set_current_thread(thread* current_thread);
  int32_t dispatch_syscall(uint64_t syscall_number, uint64_t argument0);
  bool is_current_thread_resumable() const;

private:
  process* find_process_by_handle(handle_t handle_value);
  thread* find_thread_by_handle(handle_t handle_value);
  channel* find_channel_by_handle(handle_t handle_value);
  shared_memory_object* find_shared_memory_object_by_handle(handle_t handle_value);
  kernel_object* find_object_by_handle(handle_t handle_value);
  void grant_process_access(kernel_object& object);
  bool try_translate_user_address(
    const process& owner_process, uintptr_t user_address, size_t length, uintptr_t* out_host_address) const;

  kernel_object_pool<process, USER_RUNTIME_MAX_PROCESSES> m_processes;
  kernel_object_pool<thread, USER_RUNTIME_MAX_THREADS> m_threads;
  kernel_object_pool<channel, USER_RUNTIME_MAX_CHANNELS> m_channels;
  kernel_object_pool<shared_memory_object, USER_RUNTIME_MAX_SHARED_MEMORY_OBJECTS> m_shared_memory_objects;
  thread* m_current_thread;
  handle_t m_next_handle_value = 1;
};

user_runtime& get_kernel_user_runtime();
[[noreturn]] void run_initial_user_runtime(initial_user_runtime_platform& platform);

