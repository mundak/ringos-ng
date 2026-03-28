#include "user_runtime.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "x64_pe64_image.h"

extern "C" [[noreturn]] void x64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t flags);
extern "C" void x64_syscall_entry();
extern "C" [[noreturn]] void x64_user_thread_exit();
extern "C" const uint8_t _binary_ringos_test_app_x64_pe64_image_start[];
extern "C" const uint8_t _binary_ringos_test_app_x64_pe64_image_end[];

namespace
{
  constexpr size_t PAGE_SIZE = X64_USER_IMAGE_PAGE_SIZE;
  constexpr size_t LOW_IDENTITY_SIZE = 0x400000;
  constexpr size_t LOW_PAGE_TABLE_COUNT = LOW_IDENTITY_SIZE / 0x200000;
  constexpr uintptr_t USER_IMAGE_VIRTUAL_ADDRESS = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr size_t USER_IMAGE_PAGE_COUNT = X64_USER_IMAGE_PAGE_COUNT;
  constexpr uintptr_t USER_STACK_VIRTUAL_ADDRESS = X64_USER_STACK_VIRTUAL_ADDRESS;
  constexpr size_t USER_REGION_SIZE = X64_USER_REGION_SIZE;

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
  constexpr uintptr_t USER_THREAD_INITIAL_FLAGS = 0x2;
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
    alignas(PAGE_SIZE) uint8_t user_image_pages[USER_IMAGE_PAGE_COUNT][PAGE_SIZE];
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
    void prepare_thread_launch(const process& initial_process, const thread& initial_thread);
    [[noreturn]] void enter_user_thread(const process& initial_process, const thread& initial_thread);

  private:
    void initialize_low_identity_mappings(x64_process_storage& storage);
    void initialize_user_region(x64_process_storage& storage);
    uintptr_t initialize_user_image(x64_process_storage& storage);
    void initialize_process_storage(x64_process_storage& storage);
    void initialize_syscall_msrs();

    x64_process_storage m_process_storage[USER_RUNTIME_MAX_PROCESSES] {};
    x64_cpu_local m_cpu_local {};
  };

  void initialize_x64_platform(void* context, initial_user_runtime_bootstrap& bootstrap)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->initialize(bootstrap);
  }

  void prepare_x64_platform(void* context, const process& initial_process, const thread& initial_thread)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->prepare_thread_launch(initial_process, initial_thread);
  }

  [[noreturn]] void enter_x64_platform(void* context, const process& initial_process, const thread& initial_thread)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->enter_user_thread(initial_process, initial_thread);
  }

  x64_initial_user_runtime_platform g_initial_user_runtime_platform {};
  initial_user_runtime_platform g_x64_runtime_dispatch {
    &g_initial_user_runtime_platform,
    &initialize_x64_platform,
    &prepare_x64_platform,
    &enter_x64_platform,
  };

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

    for (size_t page_index = 0; page_index < USER_IMAGE_PAGE_COUNT; ++page_index)
    {
      storage.user_page_table.entries[page_index] = make_table_entry(
        reinterpret_cast<uintptr_t>(storage.user_image_pages[page_index]), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    storage.user_page_table.entries[USER_IMAGE_PAGE_COUNT] = make_table_entry(
      reinterpret_cast<uintptr_t>(storage.user_stack_page), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  }

  uintptr_t x64_initial_user_runtime_platform::initialize_user_image(x64_process_storage& storage)
  {
    const uint8_t* const image_bytes = _binary_ringos_test_app_x64_pe64_image_start;
    const size_t image_size
      = static_cast<size_t>(_binary_ringos_test_app_x64_pe64_image_end - _binary_ringos_test_app_x64_pe64_image_start);
    uint8_t* const loaded_image = &storage.user_image_pages[0][0];
    x64_pe64_image_info image_info {};
    const x64_pe64_image_load_status load_status = load_x64_pe64_image(
      image_bytes,
      image_size,
      USER_IMAGE_VIRTUAL_ADDRESS,
      loaded_image,
      USER_IMAGE_PAGE_COUNT * PAGE_SIZE,
      &image_info);

    if (load_status != x64_pe64_image_load_status::ok)
    {
      panic(describe_x64_pe64_image_load_status(load_status));
    }

    *reinterpret_cast<uint64_t*>(storage.user_stack_page + PAGE_SIZE - sizeof(uint64_t)) = 0;
    return image_info.entry_point;
  }

  void x64_initial_user_runtime_platform::initialize_process_storage(x64_process_storage& storage)
  {
    memset(&storage, 0, sizeof(storage));
    initialize_low_identity_mappings(storage);
    initialize_user_region(storage);
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
    const uintptr_t entry_point = initialize_user_image(m_process_storage[0]);

    bootstrap.address_space.arch_root_table = reinterpret_cast<uintptr_t>(&m_process_storage[0].pml4);
    bootstrap.address_space.user_base = USER_IMAGE_VIRTUAL_ADDRESS;
    bootstrap.address_space.user_size = USER_REGION_SIZE;
    bootstrap.address_space.user_host_base = reinterpret_cast<uintptr_t>(&m_process_storage[0].user_image_pages[0][0]);
    bootstrap.thread_context.instruction_pointer = entry_point;
    bootstrap.thread_context.stack_pointer = USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE - sizeof(uint64_t);
    bootstrap.thread_context.flags = 0x202;
    bootstrap.shared_memory_address = USER_IMAGE_VIRTUAL_ADDRESS;
    bootstrap.shared_memory_size = USER_REGION_SIZE;
  }

  void x64_initial_user_runtime_platform::prepare_thread_launch(
    const process& initial_process, const thread& initial_thread)
  {
    (void) initial_process;

    m_cpu_local.user_stack_pointer = initial_thread.get_user_context().stack_pointer;
    m_cpu_local.kernel_stack_pointer = initial_thread.get_kernel_stack_top();
    initialize_syscall_msrs();
  }

  [[noreturn]] void x64_initial_user_runtime_platform::enter_user_thread(
    const process& initial_process, const thread& initial_thread)
  {
    arch_activate_process_address_space(&initial_process);

    debug_log("x64 initial user runtime ready");

    x64_enter_user_thread(
      initial_thread.get_user_context().instruction_pointer,
      initial_thread.get_user_context().stack_pointer,
      initial_thread.get_user_context().flags);
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
  };
  current_thread->set_user_context(user_context);

  const int32_t syscall_status = runtime.dispatch_syscall(frame->rax, frame->rdi);
  frame->rax = static_cast<uint64_t>(syscall_status);
  return runtime.is_current_thread_runnable();
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
  run_initial_user_runtime(g_x64_runtime_dispatch);
}
