#include "user_runtime.h"

#include "debug.h"
#include "klibc/memory.h"
#include "panic.h"
#include "x64_initial_user_runtime_platform.h"

extern "C" [[noreturn]] void x64_user_thread_exit();

namespace
{
  struct x64_syscall_frame
  {
    uint64_t r9;
    uint64_t r8;
    uint64_t r10;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rax;
    uint64_t rcx;
    uint64_t r11;
    uint64_t user_rsp;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t preserved_xmm_qwords[20];
  };

  void write_cr3(uintptr_t value)
  {
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
  }

  void load_user_thread_frame(x64_syscall_frame* frame, const thread& current_thread)
  {
    if (frame == nullptr)
    {
      return;
    }

    memset(frame, 0, sizeof(*frame));
    frame->rax = static_cast<uint64_t>(static_cast<int64_t>(current_thread.get_pending_syscall_status()));
    frame->rcx = current_thread.get_user_context().instruction_pointer;
    frame->r11 = current_thread.get_user_context().flags;
    frame->user_rsp = current_thread.get_user_context().stack_pointer;
    const uintptr_t* const preserved_registers = current_thread.get_arch_preserved_registers();
    frame->rsi = preserved_registers[X64_PRESERVED_REGISTER_RSI_INDEX];
    frame->rdi = preserved_registers[X64_PRESERVED_REGISTER_RDI_INDEX];
    frame->rbx = preserved_registers[X64_PRESERVED_REGISTER_RBX_INDEX];
    frame->rbp = preserved_registers[X64_PRESERVED_REGISTER_RBP_INDEX];
    frame->r12 = preserved_registers[X64_PRESERVED_REGISTER_R12_INDEX];
    frame->r13 = preserved_registers[X64_PRESERVED_REGISTER_R13_INDEX];
    frame->r14 = preserved_registers[X64_PRESERVED_REGISTER_R14_INDEX];
    frame->r15 = preserved_registers[X64_PRESERVED_REGISTER_R15_INDEX];
    memcpy(
      frame->preserved_xmm_qwords,
      current_thread.get_arch_preserved_simd_qwords(),
      sizeof(frame->preserved_xmm_qwords));
  }

}

void arch_activate_process_address_space(const process* process_context)
{
  if (process_context == nullptr)
  {
    return;
  }

  const uintptr_t root_table = process_context->get_address_space_info().arch_root_table;

  if (root_table == 0)
  {
    return;
  }

  write_cr3(root_table);
}

void arch_prepare_user_thread(const thread* thread_context)
{
  get_x64_initial_user_runtime_platform().prepare_user_thread(thread_context);
}

extern "C" bool x64_handle_syscall(x64_syscall_frame* frame)
{
  if (frame == nullptr)
  {
    panic("x64 syscall frame was null");
  }

  user_runtime& runtime = get_kernel_user_runtime();
  thread* current_thread = runtime.get_current_thread();

  if (current_thread == nullptr)
  {
    panic("x64 syscall without current thread");
  }

  const thread_context user_context {
    static_cast<uintptr_t>(frame->rcx),
    static_cast<uintptr_t>(frame->user_rsp),
    static_cast<uintptr_t>(frame->r11),
    0,
  };
  current_thread->set_user_context(user_context);
  uintptr_t* const preserved_registers = current_thread->get_arch_preserved_registers();
  preserved_registers[X64_PRESERVED_REGISTER_RSI_INDEX] = static_cast<uintptr_t>(frame->rsi);
  preserved_registers[X64_PRESERVED_REGISTER_RDI_INDEX] = static_cast<uintptr_t>(frame->rdi);
  preserved_registers[X64_PRESERVED_REGISTER_RBX_INDEX] = static_cast<uintptr_t>(frame->rbx);
  preserved_registers[X64_PRESERVED_REGISTER_RBP_INDEX] = static_cast<uintptr_t>(frame->rbp);
  preserved_registers[X64_PRESERVED_REGISTER_R12_INDEX] = static_cast<uintptr_t>(frame->r12);
  preserved_registers[X64_PRESERVED_REGISTER_R13_INDEX] = static_cast<uintptr_t>(frame->r13);
  preserved_registers[X64_PRESERVED_REGISTER_R14_INDEX] = static_cast<uintptr_t>(frame->r14);
  preserved_registers[X64_PRESERVED_REGISTER_R15_INDEX] = static_cast<uintptr_t>(frame->r15);
  memcpy(
    current_thread->get_arch_preserved_simd_qwords(), frame->preserved_xmm_qwords, sizeof(frame->preserved_xmm_qwords));

  const user_syscall_context syscall_context {
    frame->rax, frame->rdi, frame->rsi, frame->rdx, frame->r10, static_cast<uintptr_t>(frame->user_rsp),
  };
  const int32_t syscall_status = runtime.dispatch_syscall(syscall_context);

  if (!runtime.has_runnable_thread())
  {
    return false;
  }

  thread* const resume_thread = runtime.get_current_thread();

  if (resume_thread == nullptr)
  {
    return false;
  }

  if (resume_thread == current_thread)
  {
    resume_thread->set_pending_syscall_status(syscall_status);
  }

  load_user_thread_frame(frame, *resume_thread);
  return runtime.has_runnable_thread();
}

extern "C" [[noreturn]] void x64_user_thread_exit()
{
  debug_log("x64 returned to kernel after user runtime exit");

  while (true)
  {
  }
}

[[noreturn]] void arch_run_initial_user_runtime()
{
  initial_user_runtime_platform dispatch {
    &get_x64_initial_user_runtime_platform(),
    &initialize_x64_platform,
    &prepare_x64_platform,
    &enter_x64_platform,
  };
  run_initial_user_runtime(dispatch);
}
