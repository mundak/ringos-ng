#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "user_runtime.h"

extern "C" [[noreturn]] void x64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t flags);
extern "C" void x64_syscall_entry();
extern "C" const uint8_t x64_user_stub_start[];
extern "C" const uint8_t x64_user_stub_end[];
extern "C" [[noreturn]] void x64_user_thread_exit();

namespace
{

  constexpr uintptr_t USER_CODE_VIRTUAL_ADDRESS = 0x400000;
  constexpr uintptr_t USER_STACK_VIRTUAL_ADDRESS = 0x401000;
  constexpr size_t USER_REGION_SIZE = 0x2000;
  constexpr size_t PAGE_SIZE = 4096;
  constexpr size_t LOW_IDENTITY_SIZE = 0x400000;
  constexpr size_t LOW_PAGE_TABLE_COUNT = LOW_IDENTITY_SIZE / 0x200000;

  constexpr uint64_t PAGE_PRESENT = 1ULL << 0;
  constexpr uint64_t PAGE_WRITABLE = 1ULL << 1;
  constexpr uint64_t PAGE_USER = 1ULL << 2;

  constexpr uint32_t MSR_EFER = 0xC0000080;
  constexpr uint32_t MSR_STAR = 0xC0000081;
  constexpr uint32_t MSR_LSTAR = 0xC0000082;
  constexpr uint32_t MSR_FMASK = 0xC0000084;
  constexpr uint32_t MSR_GS_BASE = 0xC0000101;
  constexpr uint32_t MSR_KERNEL_GS_BASE = 0xC0000102;

  constexpr uint64_t EFER_SCE = 1ULL << 0;
  constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
  constexpr uint16_t USER_COMPAT_CODE_SELECTOR = 0x18;

  struct alignas(4096) page_table
  {
    uint64_t entries[512];
  };

  struct alignas(16) x64_cpu_local
  {
    uint64_t user_stack_pointer;
    uint64_t kernel_stack_pointer;
  };

  struct x64_process_storage
  {
    page_table pml4;
    page_table pdpt;
    page_table page_directory;
    page_table low_page_tables[LOW_PAGE_TABLE_COUNT];
    page_table user_page_table;
    alignas(PAGE_SIZE) uint8_t user_code_page[PAGE_SIZE];
    alignas(PAGE_SIZE) uint8_t user_stack_page[PAGE_SIZE];
  };

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
  };

  class x64_initial_user_runtime_platform final
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
    void initialize_low_identity_mappings(x64_process_storage& storage);
    void initialize_user_region(x64_process_storage& storage);
    void initialize_user_stub(x64_process_storage& storage);
    void initialize_process_storage(x64_process_storage& storage);
    void initialize_syscall_msrs();

    x64_process_storage m_process_storage[USER_RUNTIME_MAX_PROCESSES] {};
    x64_cpu_local m_cpu_local {};
  };

  void initialize_x64_platform(
    void* context,
    initial_user_runtime_bootstrap& bootstrap)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->initialize(bootstrap);
  }

  void prepare_x64_platform(
    void* context,
    const process& initial_process,
    const thread& initial_thread)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->prepare_thread_launch(initial_process, initial_thread);
  }

  [[noreturn]] void enter_x64_platform(
    void* context,
    const process& initial_process,
    const thread& initial_thread)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->enter_user_thread(initial_process, initial_thread);
  }

  x64_initial_user_runtime_platform g_initial_user_runtime_platform {};

  uint64_t make_table_entry(uintptr_t address, uint64_t flags)
  {
    return (static_cast<uint64_t>(address) & ~0xFFFULL) | flags;
  }

  void write_msr(uint32_t msr, uint64_t value)
  {
    const uint32_t low_value = static_cast<uint32_t>(value & 0xFFFFFFFFULL);
    const uint32_t high_value = static_cast<uint32_t>(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(low_value), "d"(high_value) : "memory");
  }

  uint64_t read_msr(uint32_t msr)
  {
    uint32_t low_value = 0;
    uint32_t high_value = 0;
    asm volatile("rdmsr" : "=a"(low_value), "=d"(high_value) : "c"(msr) : "memory");
    return (static_cast<uint64_t>(high_value) << 32) | low_value;
  }

  void write_cr3(uintptr_t value)
  {
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
  }

  void x64_initial_user_runtime_platform::initialize_low_identity_mappings(x64_process_storage& storage)
  {
    storage.pml4.entries[0]
      = make_table_entry(reinterpret_cast<uintptr_t>(&storage.pdpt), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    storage.pdpt.entries[0] = make_table_entry(
      reinterpret_cast<uintptr_t>(&storage.page_directory), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    for (uint32_t table_index = 0; table_index < LOW_PAGE_TABLE_COUNT; ++table_index)
    {
      page_table& current_page_table = storage.low_page_tables[table_index];
      const uintptr_t table_base_address = static_cast<uintptr_t>(table_index) * 0x200000;

      for (uint32_t entry_index = 0; entry_index < 512; ++entry_index)
      {
        const uintptr_t mapped_address = table_base_address + (static_cast<uintptr_t>(entry_index) * PAGE_SIZE);
        current_page_table.entries[entry_index] = make_table_entry(mapped_address, PAGE_PRESENT | PAGE_WRITABLE);
      }

      storage.page_directory.entries[table_index]
        = make_table_entry(reinterpret_cast<uintptr_t>(&current_page_table), PAGE_PRESENT | PAGE_WRITABLE);
    }
  }

  void x64_initial_user_runtime_platform::initialize_user_region(x64_process_storage& storage)
  {
    storage.page_directory.entries[2] = make_table_entry(
      reinterpret_cast<uintptr_t>(&storage.user_page_table), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    storage.user_page_table.entries[0]
      = make_table_entry(reinterpret_cast<uintptr_t>(storage.user_code_page), PAGE_PRESENT | PAGE_USER);
    storage.user_page_table.entries[1] = make_table_entry(
      reinterpret_cast<uintptr_t>(storage.user_stack_page), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  }

  void x64_initial_user_runtime_platform::initialize_user_stub(x64_process_storage& storage)
  {
    const size_t stub_size = static_cast<size_t>(x64_user_stub_end - x64_user_stub_start);

    if (stub_size > sizeof(storage.user_code_page))
    {
      panic("x64 user runtime stub does not fit in one page");
    }

    memset(storage.user_code_page, 0, sizeof(storage.user_code_page));
    memset(storage.user_stack_page, 0, sizeof(storage.user_stack_page));
    memcpy(storage.user_code_page, x64_user_stub_start, stub_size);
  }

  void x64_initial_user_runtime_platform::initialize_process_storage(x64_process_storage& storage)
  {
    memset(&storage, 0, sizeof(storage));
    initialize_low_identity_mappings(storage);
    initialize_user_region(storage);
    initialize_user_stub(storage);
  }

  void x64_initial_user_runtime_platform::initialize_syscall_msrs()
  {
    const uint64_t efer_value = read_msr(MSR_EFER) | EFER_SCE;
    const uint64_t star_value
      = (static_cast<uint64_t>(USER_COMPAT_CODE_SELECTOR) << 48) | (static_cast<uint64_t>(KERNEL_CODE_SELECTOR) << 32);

    write_msr(MSR_EFER, efer_value);
    write_msr(MSR_STAR, star_value);
    write_msr(MSR_LSTAR, reinterpret_cast<uintptr_t>(&x64_syscall_entry));
    write_msr(MSR_FMASK, 0x200);
    write_msr(MSR_GS_BASE, 0);
    write_msr(MSR_KERNEL_GS_BASE, reinterpret_cast<uintptr_t>(&m_cpu_local));
  }

  void x64_initial_user_runtime_platform::initialize(initial_user_runtime_bootstrap& bootstrap)
  {
    initialize_process_storage(m_process_storage[0]);

    bootstrap.address_space.arch_root_table = reinterpret_cast<uintptr_t>(&m_process_storage[0].pml4);
    bootstrap.address_space.user_base = USER_CODE_VIRTUAL_ADDRESS;
    bootstrap.address_space.user_size = USER_REGION_SIZE;
    bootstrap.thread_context.instruction_pointer = USER_CODE_VIRTUAL_ADDRESS;
    bootstrap.thread_context.stack_pointer = USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE;
    bootstrap.thread_context.flags = 0x202;
    bootstrap.shared_memory_address = USER_CODE_VIRTUAL_ADDRESS;
    bootstrap.shared_memory_size = USER_REGION_SIZE;
  }

  void x64_initial_user_runtime_platform::prepare_thread_launch(
    const process& initial_process,
    const thread& initial_thread)
  {
    (void)initial_process;

    m_cpu_local.user_stack_pointer = initial_thread.user_context().stack_pointer;
    m_cpu_local.kernel_stack_pointer = initial_thread.kernel_stack_top();
    initialize_syscall_msrs();
  }

  [[noreturn]] void x64_initial_user_runtime_platform::enter_user_thread(
    const process& initial_process,
    const thread& initial_thread)
  {
    write_cr3(initial_process.address_space_info().arch_root_table);

    debug_log("x64 initial user runtime ready");

    x64_enter_user_thread(
      initial_thread.user_context().instruction_pointer,
      initial_thread.user_context().stack_pointer,
      initial_thread.user_context().flags);
  }

}

extern "C" bool x64_handle_syscall(x64_syscall_frame* frame)
{
  if (frame == nullptr)
  {
    panic("x64 syscall frame was null");
  }

  user_runtime& runtime = kernel_user_runtime();
  thread* current_thread = runtime.current_thread();

  if (current_thread == nullptr)
  {
    panic("x64 syscall without current thread");
  }

  const thread_context user_context {
    static_cast<uintptr_t>(frame->rcx),
    static_cast<uintptr_t>(frame->user_rsp),
    static_cast<uintptr_t>(frame->r11),
  };
  current_thread->set_user_context(user_context);

  const int32_t syscall_status = runtime.dispatch_syscall(frame->rax, frame->rdi);
  frame->rax = static_cast<uint64_t>(syscall_status);
  return runtime.should_resume_current_thread();
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
    &g_initial_user_runtime_platform,
    &initialize_x64_platform,
    &prepare_x64_platform,
    &enter_x64_platform,
  };
  run_initial_user_runtime(dispatch);
}