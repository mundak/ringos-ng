#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "user_runtime.h"

extern "C" [[noreturn]] void arm64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t saved_program_status);
extern "C" const uint8_t arm64_exception_vectors[];
extern "C" const uint8_t arm64_user_stub_start[];
extern "C" const uint8_t arm64_user_stub_end[];
extern "C" [[noreturn]] void arm64_user_thread_exit();

namespace
{

  constexpr size_t PAGE_SIZE = 4096;
  constexpr size_t USER_REGION_SIZE = PAGE_SIZE * 2;
  constexpr uint32_t SVC64_EXCEPTION_CLASS = 0x15;

  struct arm64_process_storage
  {
    alignas(PAGE_SIZE) uint8_t user_code_page[PAGE_SIZE];
    alignas(PAGE_SIZE) uint8_t user_stack_page[PAGE_SIZE];
  };

  struct arm64_syscall_frame
  {
    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t elr;
    uint64_t spsr;
    uint64_t user_stack_pointer;
    uint64_t esr;
    uint64_t padding;
  };

  class arm64_initial_user_runtime_platform final
  {
  public:
    void initialize(initial_user_runtime_bootstrap& bootstrap);
    void prepare_thread_launch(
      const process& initial_process,
      const thread& initial_thread);
    [[noreturn]] void enter_user_thread(
      const process& initial_process,
      const thread& initial_thread);

  private:
    void initialize_user_stub(arm64_process_storage& storage);
    void write_vector_base(uintptr_t vector_base);

    arm64_process_storage m_process_storage[USER_RUNTIME_MAX_PROCESSES] {};
  };

  void initialize_arm64_platform(
    void* context,
    initial_user_runtime_bootstrap& bootstrap)
  {
    static_cast<arm64_initial_user_runtime_platform*>(context)->initialize(bootstrap);
  }

  void prepare_arm64_platform(
    void* context,
    const process& initial_process,
    const thread& initial_thread)
  {
    static_cast<arm64_initial_user_runtime_platform*>(context)->prepare_thread_launch(initial_process, initial_thread);
  }

  [[noreturn]] void enter_arm64_platform(
    void* context,
    const process& initial_process,
    const thread& initial_thread)
  {
    static_cast<arm64_initial_user_runtime_platform*>(context)->enter_user_thread(initial_process, initial_thread);
  }

  arm64_initial_user_runtime_platform g_initial_user_runtime_platform {};

  void arm64_initial_user_runtime_platform::initialize_user_stub(arm64_process_storage& storage)
  {
    const size_t stub_size = static_cast<size_t>(arm64_user_stub_end - arm64_user_stub_start);

    if (stub_size > sizeof(storage.user_code_page))
    {
      panic("arm64 user runtime stub does not fit in one page");
    }

    memset(storage.user_code_page, 0, sizeof(storage.user_code_page));
    memset(storage.user_stack_page, 0, sizeof(storage.user_stack_page));
    memcpy(storage.user_code_page, arm64_user_stub_start, stub_size);
  }

  void arm64_initial_user_runtime_platform::write_vector_base(uintptr_t vector_base)
  {
    asm volatile("msr vbar_el1, %0\nisb" : : "r"(vector_base) : "memory");
  }

  void arm64_initial_user_runtime_platform::initialize(initial_user_runtime_bootstrap& bootstrap)
  {
    initialize_user_stub(m_process_storage[0]);

    const uintptr_t user_base = reinterpret_cast<uintptr_t>(m_process_storage[0].user_code_page);

    bootstrap.address_space.arch_root_table = 0;
    bootstrap.address_space.user_base = user_base;
    bootstrap.address_space.user_size = USER_REGION_SIZE;
    bootstrap.thread_context.instruction_pointer = user_base;
    bootstrap.thread_context.stack_pointer = reinterpret_cast<uintptr_t>(m_process_storage[0].user_stack_page) + PAGE_SIZE;
    bootstrap.thread_context.flags = 0;
    bootstrap.shared_memory_address = user_base;
    bootstrap.shared_memory_size = USER_REGION_SIZE;
  }

  void arm64_initial_user_runtime_platform::prepare_thread_launch(
    const process& initial_process,
    const thread& initial_thread)
  {
    (void)initial_process;
    (void)initial_thread;

    write_vector_base(reinterpret_cast<uintptr_t>(arm64_exception_vectors));
  }

  [[noreturn]] void arm64_initial_user_runtime_platform::enter_user_thread(
    const process& initial_process,
    const thread& initial_thread)
  {
    (void)initial_process;

    debug_log("arm64 initial user runtime ready");

    arm64_enter_user_thread(
      initial_thread.user_context().instruction_pointer,
      initial_thread.user_context().stack_pointer,
      initial_thread.user_context().flags);
  }

}

extern "C" bool arm64_handle_syscall(arm64_syscall_frame* frame)
{
  if (frame == nullptr)
  {
    panic("arm64 syscall frame was null");
  }

  if ((frame->esr >> 26) != SVC64_EXCEPTION_CLASS)
  {
    panic("arm64 received an unexpected lower-el synchronous exception");
  }

  user_runtime& runtime = kernel_user_runtime();
  thread* current_thread = runtime.current_thread();

  if (current_thread == nullptr)
  {
    panic("arm64 syscall without current thread");
  }

  const thread_context user_context {
    static_cast<uintptr_t>(frame->elr),
    static_cast<uintptr_t>(frame->user_stack_pointer),
    static_cast<uintptr_t>(frame->spsr),
  };
  current_thread->set_user_context(user_context);

  const int32_t syscall_status = runtime.dispatch_syscall(frame->x8, frame->x0);
  frame->x0 = static_cast<uint64_t>(syscall_status);
  return runtime.should_resume_current_thread();
}

extern "C" [[noreturn]] void arm64_unhandled_exception()
{
  panic("arm64 hit an unhandled exception vector");
}

extern "C" [[noreturn]] void arm64_user_thread_exit()
{
  debug_log("arm64 returned to kernel after user runtime exit");

  while (true)
  {
  }
}

[[noreturn]] void arch_run_initial_user_runtime()
{
  initial_user_runtime_platform dispatch {
    &g_initial_user_runtime_platform,
    &initialize_arm64_platform,
    &prepare_arm64_platform,
    &enter_arm64_platform,
  };
  run_initial_user_runtime(dispatch);
}