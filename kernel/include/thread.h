#pragma once

#include "kernel_object.h"
#include "user_runtime_types.h"

class process;

class thread final : public kernel_object
{
public:
  process* process_context() const;
  user_thread_state state() const;
  const thread_context& user_context() const;
  uint64_t exit_status() const;
  uintptr_t kernel_stack_top() const;

  void set_user_context(const thread_context& user_context);
  void set_state(user_thread_state state);
  void set_exit_status(uint64_t exit_status);

private:
  friend class user_runtime;

  void clear();
  void activate(
    uint32_t thread_id,
    uint64_t handle_value,
    process& process_context,
    const thread_context& initial_context);

  process* m_process;
  user_thread_state m_state;
  thread_context m_user_context;
  uint64_t m_exit_status;
  alignas(16) uint8_t m_kernel_stack[USER_RUNTIME_KERNEL_STACK_SIZE];
};