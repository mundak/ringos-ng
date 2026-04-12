#include "x64_initial_user_runtime_platform.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "klibc/memory.h"
#include "panic.h"
#include "x64_windows_compat.h"

extern "C" [[noreturn]] void x64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t flags);
extern "C" void x64_syscall_entry();
extern "C" const uint8_t _binary_ringos_test_app_image_start[];
extern "C" const uint8_t _binary_ringos_test_app_image_end[];

namespace
{
  constexpr size_t PAGE_SIZE = X64_USER_IMAGE_PAGE_SIZE;
  constexpr uintptr_t USER_IMAGE_VIRTUAL_ADDRESS = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr uintptr_t USER_STACK_VIRTUAL_ADDRESS = X64_USER_STACK_VIRTUAL_ADDRESS;
  constexpr size_t USER_REGION_SIZE = X64_USER_REGION_SIZE;
  constexpr size_t WINDOWS_X64_STACK_HOME_SPACE_SIZE = 32;

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
  constexpr uint64_t USER_THREAD_INITIAL_FLAGS = 0x2;
  constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
  constexpr uint16_t USER_COMPAT_CODE_SELECTOR = 0x18;

  x64_initial_user_runtime_platform g_initial_user_runtime_platform {};

  bool resolve_initial_windows_import(
    void* context, const char* dll_name, const char* function_name, uint32_t* out_syscall_number)
  {
    (void) context;
    return try_resolve_x64_windows_import(dll_name, function_name, out_syscall_number);
  }

  constexpr x64_pe64_import_resolver INITIAL_WINDOWS_IMPORT_RESOLVER {
    nullptr,
    &resolve_initial_windows_import,
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
}

x64_initial_user_runtime_platform& get_x64_initial_user_runtime_platform()
{
  return g_initial_user_runtime_platform;
}

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

void x64_initial_user_runtime_platform::initialize_low_identity_mappings(x64_process_storage& storage)
{
  storage.pml4.entries[0]
    = make_table_entry(reinterpret_cast<uintptr_t>(&storage.pdpt), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  storage.pdpt.entries[0]
    = make_table_entry(reinterpret_cast<uintptr_t>(&storage.page_directory), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

  for (uint32_t table_index = 0; table_index < X64_LOW_PAGE_TABLE_COUNT; ++table_index)
  {
    x64_page_table& current_page_table = storage.low_page_tables[table_index];
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
  storage.page_directory.entries[2]
    = make_table_entry(reinterpret_cast<uintptr_t>(&storage.user_page_table), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

  for (size_t page_index = 0; page_index < X64_USER_IMAGE_PAGE_COUNT; ++page_index)
  {
    storage.user_page_table.entries[page_index] = make_table_entry(
      reinterpret_cast<uintptr_t>(storage.user_image_pages[page_index]), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
  }

  storage.user_page_table.entries[X64_USER_IMAGE_PAGE_COUNT]
    = make_table_entry(reinterpret_cast<uintptr_t>(storage.user_stack_page), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
}

uintptr_t x64_initial_user_runtime_platform::initialize_user_image(
  x64_process_storage& storage, const uint8_t* image_start, const uint8_t* image_end)
{
  const uint8_t* const image_bytes = image_start;
  const size_t image_size = static_cast<size_t>(image_end - image_start);
  uint8_t* const loaded_image = &storage.user_image_pages[0][0];
  x64_pe64_image_info image_info {};
  const x64_pe64_image_load_status load_status = load_x64_pe64_image(
    image_bytes,
    image_size,
    USER_IMAGE_VIRTUAL_ADDRESS,
    loaded_image,
    USER_REGION_SIZE,
    &INITIAL_WINDOWS_IMPORT_RESOLVER,
    &image_info);

  if (load_status != X64_PE64_IMAGE_LOAD_STATUS_OK)
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

void x64_initial_user_runtime_platform::populate_bootstrap_for_process(
  x64_process_storage& storage,
  const uint8_t* image_start,
  const uint8_t* image_end,
  address_space& address_space_info,
  thread_context& thread_context_info)
{
  initialize_process_storage(storage);
  const uintptr_t entry_point = initialize_user_image(storage, image_start, image_end);

  address_space_info.arch_root_table = reinterpret_cast<uintptr_t>(&storage.pml4);
  address_space_info.user_base = USER_IMAGE_VIRTUAL_ADDRESS;
  address_space_info.user_size = USER_REGION_SIZE;
  address_space_info.user_host_base = reinterpret_cast<uintptr_t>(&storage.user_image_pages[0][0]);

  thread_context_info.instruction_pointer = entry_point;
  thread_context_info.stack_pointer
    = USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE - sizeof(uint64_t) - WINDOWS_X64_STACK_HOME_SPACE_SIZE;
  thread_context_info.flags = USER_THREAD_INITIAL_FLAGS;
  thread_context_info.argument0 = 0;
}

void x64_initial_user_runtime_platform::initialize(initial_user_runtime_bootstrap& bootstrap)
{
  memset(&bootstrap, 0, sizeof(bootstrap));
  bootstrap.process_count = 1;
  bootstrap.initial_process_index = 0;
  populate_bootstrap_for_process(
    m_process_storage[0],
    _binary_ringos_test_app_image_start,
    _binary_ringos_test_app_image_end,
    bootstrap.address_space[0],
    bootstrap.thread_context[0]);
}

void x64_initial_user_runtime_platform::prepare_thread_launch(
  const process& initial_process, const thread& initial_thread)
{
  (void) initial_process;

  prepare_user_thread(&initial_thread);
  initialize_syscall_msrs();
}

void x64_initial_user_runtime_platform::prepare_user_thread(const thread* current_thread)
{
  if (current_thread == nullptr)
  {
    return;
  }

  m_cpu_local.user_stack_pointer = current_thread->get_user_context().stack_pointer;
  m_cpu_local.kernel_stack_pointer = current_thread->get_kernel_stack_top();
}

[[noreturn]] void x64_initial_user_runtime_platform::enter_user_thread(
  const process& initial_process, const thread& initial_thread)
{
  arch_activate_process_address_space(&initial_process);
  prepare_user_thread(&initial_thread);

  debug_log("x64 initial user runtime ready");

  x64_enter_user_thread(
    initial_thread.get_user_context().instruction_pointer,
    initial_thread.get_user_context().stack_pointer,
    initial_thread.get_user_context().flags);
}
