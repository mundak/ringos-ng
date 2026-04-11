#include "thread.h"

#include "klibc/memory.h"
#include "process.h"

thread::thread(
  uint32_t thread_id, handle_t handle_value, process& process_context, const thread_context& initial_context)
  : kernel_object(thread_id, handle_value)
  , m_process(&process_context)
  , m_state(USER_THREAD_STATE_READY)
  , m_user_context(initial_context)
  , m_exit_status(0)
  , m_initial_argument0(initial_context.argument0)
  , m_should_deliver_initial_argument(true)
  , m_pending_syscall_status(STATUS_OK)
{
  memset(m_arch_preserved_registers, 0, sizeof(m_arch_preserved_registers));
  memset(m_arch_preserved_simd_qwords, 0, sizeof(m_arch_preserved_simd_qwords));
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

uintptr_t thread::get_initial_argument0() const
{
  return m_initial_argument0;
}

bool thread::has_initial_argument() const
{
  return m_should_deliver_initial_argument;
}

int32_t thread::get_pending_syscall_status() const
{
  return m_pending_syscall_status;
}

const uintptr_t* thread::get_arch_preserved_registers() const
{
  return m_arch_preserved_registers;
}

uintptr_t* thread::get_arch_preserved_registers()
{
  return m_arch_preserved_registers;
}

const uint64_t* thread::get_arch_preserved_simd_qwords() const
{
  return m_arch_preserved_simd_qwords;
}

uint64_t* thread::get_arch_preserved_simd_qwords()
{
  return m_arch_preserved_simd_qwords;
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

void thread::clear_initial_argument()
{
  m_should_deliver_initial_argument = false;
}

void thread::set_pending_syscall_status(int32_t status)
{
  m_pending_syscall_status = status;
}
