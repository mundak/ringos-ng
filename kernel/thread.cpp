#include "thread.h"

#include "memory.h"
#include "process.h"

thread::thread(
  uint32_t thread_id, handle_t handle_value, process& process_context, const thread_context& initial_context)
  : kernel_object(thread_id, handle_value)
  , m_process(&process_context)
  , m_state(user_thread_state::ready)
  , m_user_context(initial_context)
  , m_exit_status(0)
{
  memset(m_kernel_stack, 0, sizeof(m_kernel_stack));
}

process* thread::get_process_context() const
{
  return m_process;
}

user_thread_state thread::get_state() const
{
  return m_state;
}

const thread_context& thread::get_user_context() const
{
  return m_user_context;
}

uint64_t thread::get_exit_status() const
{
  return m_exit_status;
}

uintptr_t thread::get_kernel_stack_top() const
{
  return reinterpret_cast<uintptr_t>(m_kernel_stack) + sizeof(m_kernel_stack);
}

void thread::set_user_context(const thread_context& user_context)
{
  m_user_context = user_context;
}

void thread::set_state(user_thread_state state)
{
  m_state = state;
}

void thread::set_exit_status(uint64_t exit_status)
{
  m_exit_status = exit_status;
}
