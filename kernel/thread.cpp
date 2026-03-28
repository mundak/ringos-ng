#include "thread.h"

#include "memory.h"
#include "process.h"

process* thread::process_context() const
{
  return m_process;
}

user_thread_state thread::state() const
{
  return m_state;
}

const thread_context& thread::user_context() const
{
  return m_user_context;
}

uint64_t thread::exit_status() const
{
  return m_exit_status;
}

uintptr_t thread::kernel_stack_top() const
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

void thread::clear()
{
  clear_identity();
  m_process = nullptr;
  m_state = user_thread_state::empty;
  memset(&m_user_context, 0, sizeof(m_user_context));
  m_exit_status = 0;
  memset(m_kernel_stack, 0, sizeof(m_kernel_stack));
}

void thread::activate(
  uint32_t thread_id,
  uint64_t handle_value,
  process& process_context,
  const thread_context& initial_context)
{
  clear();
  activate_identity(thread_id, handle_value);
  m_process = &process_context;
  m_state = user_thread_state::ready;
  m_user_context = initial_context;
}