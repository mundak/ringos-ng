#pragma once

#include "kernel_object.h"
#include "user_runtime_types.h"

class process;

class thread final : public kernel_object
{
public:
  thread(uint32_t thread_id, handle_t handle_value, process& process_context, const thread_context& initial_context);
  process* get_process_context() const;
  user_thread_state get_state() const;
  const thread_context& get_user_context() const;
  const user_thread_resume& get_resume_state() const;
  uint64_t get_exit_status() const;
  uintptr_t get_kernel_stack_top() const;
  uintptr_t get_initial_argument0() const;
  bool has_initial_argument() const;
  int32_t get_pending_syscall_status() const;
  const uintptr_t* get_arch_preserved_registers() const;
  uintptr_t* get_arch_preserved_registers();
  const uint64_t* get_arch_preserved_simd_qwords() const;
  uint64_t* get_arch_preserved_simd_qwords();

  void set_user_context(const thread_context& user_context);
  void set_state(user_thread_state state);
  void set_exit_status(uint64_t exit_status);
  void clear_initial_argument();
  void set_pending_syscall_status(int32_t status);
  void prepare_syscall_resume(int32_t status);
  void prepare_rpc_resume(
    uintptr_t callback_address, uintptr_t completion_address, uintptr_t argument0, uintptr_t stack_pointer);

private:
  void sync_resume_to_user_context();

  process* m_process;
  user_thread_state m_state;
  thread_context m_user_context;
  user_thread_resume m_resume_state;
  uint64_t m_exit_status;
  uintptr_t m_initial_argument0;
  bool m_should_deliver_initial_argument;
  int32_t m_pending_syscall_status;
  uintptr_t m_arch_preserved_registers[USER_THREAD_ARCH_PRESERVED_REGISTER_COUNT];
  uint64_t m_arch_preserved_simd_qwords[USER_THREAD_ARCH_PRESERVED_SIMD_QWORD_COUNT];
  alignas(16) uint8_t m_kernel_stack[USER_RUNTIME_KERNEL_STACK_SIZE];
};
