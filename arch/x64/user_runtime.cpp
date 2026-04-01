#include "user_runtime.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "x64_pe64_image.h"
#include "x64_windows_compat.h"

extern "C" [[noreturn]] void x64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t flags);
extern "C" void x64_syscall_entry();
extern "C" [[noreturn]] void x64_user_thread_exit();
extern "C" const uint8_t _binary_ringos_console_driver_image_start[];
extern "C" const uint8_t _binary_ringos_console_driver_image_end[];
extern "C" const uint8_t _binary_ringos_console_client_image_start[];
extern "C" const uint8_t _binary_ringos_console_client_image_end[];

namespace
{
  constexpr size_t PAGE_SIZE = X64_USER_IMAGE_PAGE_SIZE;
  constexpr size_t LOW_IDENTITY_SIZE = 0x400000;
  constexpr size_t LOW_PAGE_TABLE_COUNT = LOW_IDENTITY_SIZE / 0x200000;
  constexpr uintptr_t USER_IMAGE_VIRTUAL_ADDRESS = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr size_t USER_IMAGE_PAGE_COUNT = X64_USER_IMAGE_PAGE_COUNT;
  constexpr uintptr_t USER_STACK_VIRTUAL_ADDRESS = X64_USER_STACK_VIRTUAL_ADDRESS;
  constexpr uintptr_t USER_RPC_TRANSFER_VIRTUAL_ADDRESS = X64_USER_RPC_TRANSFER_VIRTUAL_ADDRESS;
  constexpr uintptr_t USER_DEVICE_MEMORY_VIRTUAL_ADDRESS = X64_USER_DEVICE_MEMORY_VIRTUAL_ADDRESS;
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
  constexpr uint32_t X64_PRESERVED_REGISTER_RSI_INDEX = 0;
  constexpr uint32_t X64_PRESERVED_REGISTER_RDI_INDEX = 1;
  constexpr uint32_t X64_PRESERVED_REGISTER_RBX_INDEX = 2;
  constexpr uint32_t X64_PRESERVED_REGISTER_RBP_INDEX = 3;
  constexpr uint32_t X64_PRESERVED_REGISTER_R12_INDEX = 4;
  constexpr uint32_t X64_PRESERVED_REGISTER_R13_INDEX = 5;
  constexpr uint32_t X64_PRESERVED_REGISTER_R14_INDEX = 6;
  constexpr uint32_t X64_PRESERVED_REGISTER_R15_INDEX = 7;

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
    alignas(PAGE_SIZE) uint8_t user_rpc_transfer_page[PAGE_SIZE];
    alignas(PAGE_SIZE) uint8_t user_device_memory_page[PAGE_SIZE];
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
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t preserved_xmm_qwords[20];
  };

  class x64_initial_user_runtime_platform final
  {
  public:
    void initialize(initial_user_runtime_bootstrap& bootstrap);
    void prepare_thread_launch(const process& initial_process, const thread& initial_thread);
    [[noreturn]] void enter_user_thread(const process& initial_process, const thread& initial_thread);
    void prepare_user_thread(const thread* current_thread);

  private:
    void initialize_low_identity_mappings(x64_process_storage& storage);
    void initialize_user_region(x64_process_storage& storage);
    uintptr_t initialize_user_image(x64_process_storage& storage, const uint8_t* image_start, const uint8_t* image_end);
    void initialize_process_storage(x64_process_storage& storage);
    void initialize_syscall_msrs();
    void populate_bootstrap_for_process(
      x64_process_storage& storage,
      const uint8_t* image_start,
      const uint8_t* image_end,
      address_space& address_space_info,
      thread_context& thread_context_info);

    x64_process_storage m_process_storage[USER_RUNTIME_MAX_INITIAL_PROCESSES] {};
    x64_cpu_local m_cpu_local {};
  };

  void initialize_x64_platform(void* context, initial_user_runtime_bootstrap& bootstrap)
  {
    static_cast<x64_initial_user_runtime_platform*>(context)->initialize(bootstrap);
  }

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
    storage.user_page_table.entries[USER_IMAGE_PAGE_COUNT + 1] = make_table_entry(
      reinterpret_cast<uintptr_t>(storage.user_rpc_transfer_page), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    storage.user_page_table.entries[USER_IMAGE_PAGE_COUNT + 2] = make_table_entry(
      reinterpret_cast<uintptr_t>(storage.user_device_memory_page), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
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
    address_space_info.rpc_transfer_user_address = USER_RPC_TRANSFER_VIRTUAL_ADDRESS;
    address_space_info.rpc_transfer_host_address = reinterpret_cast<uintptr_t>(&storage.user_rpc_transfer_page[0]);
    address_space_info.rpc_transfer_size = PAGE_SIZE;
    address_space_info.device_memory_user_address = USER_DEVICE_MEMORY_VIRTUAL_ADDRESS;
    address_space_info.device_memory_host_address = reinterpret_cast<uintptr_t>(&storage.user_device_memory_page[0]);
    address_space_info.device_memory_size = PAGE_SIZE;

    thread_context_info.instruction_pointer = entry_point;
    thread_context_info.stack_pointer
      = USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE - sizeof(uint64_t) - WINDOWS_X64_STACK_HOME_SPACE_SIZE;
    thread_context_info.flags = USER_THREAD_INITIAL_FLAGS;
    thread_context_info.argument0 = 0;
  }

  void x64_initial_user_runtime_platform::initialize(initial_user_runtime_bootstrap& bootstrap)
  {
    memset(&bootstrap, 0, sizeof(bootstrap));
    bootstrap.process_count = 2;
    bootstrap.initial_process_index = 0;
    populate_bootstrap_for_process(
      m_process_storage[0],
      _binary_ringos_console_driver_image_start,
      _binary_ringos_console_driver_image_end,
      bootstrap.address_space[0],
      bootstrap.thread_context[0]);
    populate_bootstrap_for_process(
      m_process_storage[1],
      _binary_ringos_console_client_image_start,
      _binary_ringos_console_client_image_end,
      bootstrap.address_space[1],
      bootstrap.thread_context[1]);
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
  g_initial_user_runtime_platform.prepare_user_thread(thread_context);
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
  run_initial_user_runtime(g_x64_runtime_dispatch);
}
