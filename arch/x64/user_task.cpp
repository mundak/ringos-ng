#include "arch_user_task.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "stage1_kernel.h"

extern "C" [[noreturn]] void x64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t flags);
extern "C" void x64_syscall_entry();
extern "C" const uint8_t x64_user_stub_start[];
extern "C" const uint8_t x64_user_stub_end[];

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
    uint64_t m_entries[512];
  };

  struct alignas(16) x64_cpu_local
  {
    uint64_t m_user_stack_pointer;
    uint64_t m_kernel_stack_pointer;
  };

  struct x64_process_storage
  {
    page_table m_pml4;
    page_table m_pdpt;
    page_table m_page_directory;
    page_table m_low_page_tables[LOW_PAGE_TABLE_COUNT];
    page_table m_user_page_table;
    alignas(PAGE_SIZE) uint8_t m_user_code_page[PAGE_SIZE];
    alignas(PAGE_SIZE) uint8_t m_user_stack_page[PAGE_SIZE];
  };

  struct x64_syscall_frame
  {
    uint64_t m_r9;
    uint64_t m_r8;
    uint64_t m_r10;
    uint64_t m_rdx;
    uint64_t m_rsi;
    uint64_t m_rdi;
    uint64_t m_rax;
    uint64_t m_rcx;
    uint64_t m_r11;
    uint64_t m_user_rsp;
  };

  x64_process_storage g_process_storage[STAGE1_MAX_PROCESSES] {};
  x64_cpu_local g_cpu_local {};

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

  uintptr_t thread_kernel_stack_top(const thread& current_thread)
  {
    return reinterpret_cast<uintptr_t>(current_thread.m_kernel_stack) + sizeof(current_thread.m_kernel_stack);
  }

  void initialize_low_identity_mappings(x64_process_storage& storage)
  {
    storage.m_pml4.m_entries[0]
      = make_table_entry(reinterpret_cast<uintptr_t>(&storage.m_pdpt), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    storage.m_pdpt.m_entries[0] = make_table_entry(
      reinterpret_cast<uintptr_t>(&storage.m_page_directory), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    for (uint32_t table_index = 0; table_index < LOW_PAGE_TABLE_COUNT; ++table_index)
    {
      page_table& current_page_table = storage.m_low_page_tables[table_index];
      const uintptr_t table_base_address = static_cast<uintptr_t>(table_index) * 0x200000;

      for (uint32_t entry_index = 0; entry_index < 512; ++entry_index)
      {
        const uintptr_t mapped_address = table_base_address + (static_cast<uintptr_t>(entry_index) * PAGE_SIZE);
        current_page_table.m_entries[entry_index] = make_table_entry(mapped_address, PAGE_PRESENT | PAGE_WRITABLE);
      }

      storage.m_page_directory.m_entries[table_index]
        = make_table_entry(reinterpret_cast<uintptr_t>(&current_page_table), PAGE_PRESENT | PAGE_WRITABLE);
    }
  }

  void initialize_user_region(x64_process_storage& storage)
  {
    storage.m_page_directory.m_entries[2] = make_table_entry(
      reinterpret_cast<uintptr_t>(&storage.m_user_page_table), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    storage.m_user_page_table.m_entries[0]
      = make_table_entry(reinterpret_cast<uintptr_t>(storage.m_user_code_page), PAGE_PRESENT | PAGE_USER);
    storage.m_user_page_table.m_entries[1] = make_table_entry(
      reinterpret_cast<uintptr_t>(storage.m_user_stack_page), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  }

  void initialize_user_stub(x64_process_storage& storage)
  {
    const size_t stub_size = static_cast<size_t>(x64_user_stub_end - x64_user_stub_start);

    if (stub_size > sizeof(storage.m_user_code_page))
    {
      panic("x64 user stub does not fit in one page");
    }

    memset(storage.m_user_code_page, 0, sizeof(storage.m_user_code_page));
    memset(storage.m_user_stack_page, 0, sizeof(storage.m_user_stack_page));
    memcpy(storage.m_user_code_page, x64_user_stub_start, stub_size);
  }

  void initialize_process_storage(x64_process_storage& storage)
  {
    memset(&storage, 0, sizeof(storage));
    initialize_low_identity_mappings(storage);
    initialize_user_region(storage);
    initialize_user_stub(storage);
  }

  void initialize_syscall_msrs()
  {
    const uint64_t efer_value = read_msr(MSR_EFER) | EFER_SCE;
    const uint64_t star_value
      = (static_cast<uint64_t>(USER_COMPAT_CODE_SELECTOR) << 48) | (static_cast<uint64_t>(KERNEL_CODE_SELECTOR) << 32);

    write_msr(MSR_EFER, efer_value);
    write_msr(MSR_STAR, star_value);
    write_msr(MSR_LSTAR, reinterpret_cast<uintptr_t>(&x64_syscall_entry));
    write_msr(MSR_FMASK, 0x200);
    write_msr(MSR_GS_BASE, 0);
    write_msr(MSR_KERNEL_GS_BASE, reinterpret_cast<uintptr_t>(&g_cpu_local));
  }

}

extern "C" bool x64_handle_syscall(x64_syscall_frame* frame)
{
  if (frame == nullptr)
  {
    panic("x64 syscall frame was null");
  }

  thread* current_thread = stage1_get_current_thread();

  if (current_thread == nullptr)
  {
    panic("x64 syscall without current thread");
  }

  current_thread->m_user_context.m_instruction_pointer = static_cast<uintptr_t>(frame->m_rcx);
  current_thread->m_user_context.m_stack_pointer = static_cast<uintptr_t>(frame->m_user_rsp);
  current_thread->m_user_context.m_flags = static_cast<uintptr_t>(frame->m_r11);

  const int32_t syscall_status = stage1_dispatch_syscall(frame->m_rax, frame->m_rdi);
  frame->m_rax = static_cast<uint64_t>(syscall_status);
  return stage1_should_resume_current_thread();
}

extern "C" [[noreturn]] void x64_stage1_thread_exit()
{
  debug_log("stage1 returned to kernel after exit syscall");

  while (true)
  {
  }
}

[[noreturn]] void arch_run_initial_user_task()
{
  stage1_reset_kernel_state();
  initialize_process_storage(g_process_storage[0]);

  address_space address_space_info;
  memset(&address_space_info, 0, sizeof(address_space_info));
  address_space_info.m_arch_root_table = reinterpret_cast<uintptr_t>(&g_process_storage[0].m_pml4);
  address_space_info.m_user_base = USER_CODE_VIRTUAL_ADDRESS;
  address_space_info.m_user_size = USER_REGION_SIZE;

  process* initial_process = stage1_create_process(address_space_info);

  if (initial_process == nullptr)
  {
    panic("failed to create stage1 process");
  }

  uint64_t thread_handle = 0;
  const thread_context initial_thread_context {
    USER_CODE_VIRTUAL_ADDRESS,
    USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE,
    0x202,
  };
  thread* initial_thread = stage1_create_thread(*initial_process, initial_thread_context, &thread_handle);

  if (initial_thread == nullptr || thread_handle == 0)
  {
    panic("failed to create stage1 thread");
  }

  uint64_t first_channel_handle = 0;
  uint64_t second_channel_handle = 0;

  if (!stage1_create_channel_pair(*initial_process, &first_channel_handle, &second_channel_handle))
  {
    panic("failed to create stage1 channel pair");
  }

  uint64_t shared_memory_handle = 0;
  shared_memory_object* shared_memory = stage1_create_shared_memory_object(
    *initial_process, USER_CODE_VIRTUAL_ADDRESS, USER_REGION_SIZE, &shared_memory_handle);

  if (shared_memory == nullptr || shared_memory_handle == 0)
  {
    panic("failed to create stage1 shared memory object");
  }

  if (first_channel_handle == 0 || second_channel_handle == 0)
  {
    panic("failed to install stage1 channel handles");
  }

  stage1_set_current_thread(initial_thread);
  g_cpu_local.m_user_stack_pointer = initial_thread->m_user_context.m_stack_pointer;
  g_cpu_local.m_kernel_stack_pointer = thread_kernel_stack_top(*initial_thread);

  initialize_syscall_msrs();
  write_cr3(initial_process->m_address_space.m_arch_root_table);

  debug_log("stage1 x64 kernel objects ready");

  x64_enter_user_thread(
    initial_thread->m_user_context.m_instruction_pointer,
    initial_thread->m_user_context.m_stack_pointer,
    initial_thread->m_user_context.m_flags);
}