#include "user_runtime.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
#include "x64_emulator.h"
#include "x64_pe64_image.h"
#include "x64_windows_compat.h"

extern "C" const uint8_t arm64_exception_vectors[];
extern "C" const uint8_t _binary_ringos_test_app_x64_pe64_image_start[];
extern "C" const uint8_t _binary_ringos_test_app_x64_pe64_image_end[];
extern "C" [[noreturn]] void arm64_user_thread_exit();

namespace
{
  constexpr size_t PAGE_SIZE = 4096;
  constexpr size_t LARGE_PAGE_SIZE = 0x200000;
  constexpr size_t KERNEL_IDENTITY_SIZE = 0x2000000;
  constexpr uint32_t KERNEL_BLOCK_COUNT = static_cast<uint32_t>(KERNEL_IDENTITY_SIZE / LARGE_PAGE_SIZE);
  constexpr uintptr_t USER_REGION_VIRTUAL_ADDRESS = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr uintptr_t KERNEL_IDENTITY_BASE = 0x40000000;
  constexpr size_t USER_REGION_SIZE = LARGE_PAGE_SIZE;
  constexpr uint32_t USER_BLOCK_INDEX = static_cast<uint32_t>(USER_REGION_VIRTUAL_ADDRESS / LARGE_PAGE_SIZE);
  constexpr uint32_t KERNEL_ROOT_INDEX = static_cast<uint32_t>(KERNEL_IDENTITY_BASE >> 30);
  constexpr uint64_t X64_INITIAL_RFLAGS = 0x202;
  constexpr uint64_t X64_EMULATOR_INSTRUCTION_BUDGET = 512;

  constexpr uint64_t TABLE_DESCRIPTOR = 0x3;
  constexpr uint64_t BLOCK_DESCRIPTOR = 0x1;
  constexpr uint64_t ATTRIBUTE_INDEX_NORMAL = 0ULL << 2;
  constexpr uint64_t ACCESS_PERMISSION_USER_RW = 1ULL << 6;
  constexpr uint64_t INNER_SHAREABLE = 3ULL << 8;
  constexpr uint64_t ACCESS_FLAG = 1ULL << 10;

  constexpr uint64_t SCTLR_MMU_ENABLE = 1ULL << 0;
  constexpr uint64_t SCTLR_DATA_CACHE_ENABLE = 1ULL << 2;
  constexpr uint64_t SCTLR_INSTRUCTION_CACHE_ENABLE = 1ULL << 12;

  constexpr uint64_t TCR_T0SZ_39_BIT_VA = 25ULL;
  constexpr uint64_t TCR_IRGN0_WRITE_BACK = 1ULL << 8;
  constexpr uint64_t TCR_ORGN0_WRITE_BACK = 1ULL << 10;
  constexpr uint64_t TCR_SH0_INNER_SHAREABLE = 3ULL << 12;
  constexpr uint64_t TCR_TG0_4KB = 0ULL << 14;
  constexpr uint64_t TCR_EPD1_DISABLE = 1ULL << 23;

  constexpr uint64_t MAIR_ATTRIBUTE_NORMAL_WRITE_BACK = 0xFFULL;

  struct alignas(PAGE_SIZE) translation_table
  {
    uint64_t entries[512];
  };

  struct arm64_process_storage
  {
    translation_table root_table;
    translation_table lower_block_table;
    translation_table kernel_block_table;
    alignas(LARGE_PAGE_SIZE) uint8_t user_region[USER_REGION_SIZE];
  };

  class arm64_initial_user_runtime_platform final
  {
  public:
    void initialize(initial_user_runtime_bootstrap& bootstrap);
    void prepare_thread_launch(const process& initial_process, const thread& initial_thread);
    [[noreturn]] void enter_user_thread(const process& initial_process, const thread& initial_thread);
    void activate_process_address_space(const process* process_context);

  private:
    static int32_t dispatch_x64_syscall(void* context, const x64_emulator_state& state, bool* out_should_continue);
    void enable_mmu();
    uintptr_t initialize_user_image(arm64_process_storage& storage);
    void initialize_process_storage(arm64_process_storage& storage);
    void initialize_translation_tables(arm64_process_storage& storage);
    void invalidate_tlb();
    uint64_t read_system_control() const;
    void write_mair(uintptr_t value);
    void write_system_control(uint64_t value);
    void write_tcr(uint64_t value);
    void write_ttbr0(uintptr_t value);
    void write_vector_base(uintptr_t vector_base);

    arm64_process_storage m_process_storage[USER_RUNTIME_MAX_PROCESSES] {};
    bool m_mmu_enabled = false;
  };

  void initialize_arm64_platform(void* context, initial_user_runtime_bootstrap& bootstrap)
  {
    static_cast<arm64_initial_user_runtime_platform*>(context)->initialize(bootstrap);
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

  void prepare_arm64_platform(void* context, const process& initial_process, const thread& initial_thread)
  {
    static_cast<arm64_initial_user_runtime_platform*>(context)->prepare_thread_launch(initial_process, initial_thread);
  }

  [[noreturn]] void enter_arm64_platform(void* context, const process& initial_process, const thread& initial_thread)
  {
    static_cast<arm64_initial_user_runtime_platform*>(context)->enter_user_thread(initial_process, initial_thread);
  }

  arm64_initial_user_runtime_platform g_initial_user_runtime_platform {};

  static_assert((KERNEL_IDENTITY_SIZE % LARGE_PAGE_SIZE) == 0);
  static_assert((USER_REGION_VIRTUAL_ADDRESS % LARGE_PAGE_SIZE) == 0);
  static_assert(USER_BLOCK_INDEX < 512);
  static_assert(KERNEL_ROOT_INDEX < 512);

  uint64_t make_table_descriptor(uintptr_t address)
  {
    return (static_cast<uint64_t>(address) & ~0xFFFULL) | TABLE_DESCRIPTOR;
  }

  uint64_t make_block_descriptor(uintptr_t address, uint64_t flags)
  {
    return (static_cast<uint64_t>(address) & ~0x1FFFFFULL) | flags | BLOCK_DESCRIPTOR;
  }

  void arm64_initial_user_runtime_platform::initialize_process_storage(arm64_process_storage& storage)
  {
    memset(&storage, 0, sizeof(storage));
  }

  void arm64_initial_user_runtime_platform::initialize_translation_tables(arm64_process_storage& storage)
  {
    storage.root_table.entries[0] = make_table_descriptor(reinterpret_cast<uintptr_t>(&storage.lower_block_table));
    storage.root_table.entries[KERNEL_ROOT_INDEX]
      = make_table_descriptor(reinterpret_cast<uintptr_t>(&storage.kernel_block_table));
    storage.lower_block_table.entries[USER_BLOCK_INDEX] = make_block_descriptor(
      reinterpret_cast<uintptr_t>(storage.user_region),
      ATTRIBUTE_INDEX_NORMAL | ACCESS_PERMISSION_USER_RW | INNER_SHAREABLE | ACCESS_FLAG);

    for (uint32_t block_index = 0; block_index < KERNEL_BLOCK_COUNT; ++block_index)
    {
      storage.kernel_block_table.entries[block_index] = make_block_descriptor(
        KERNEL_IDENTITY_BASE + (static_cast<uintptr_t>(block_index) * LARGE_PAGE_SIZE),
        ATTRIBUTE_INDEX_NORMAL | INNER_SHAREABLE | ACCESS_FLAG);
    }
  }

  uintptr_t arm64_initial_user_runtime_platform::initialize_user_image(arm64_process_storage& storage)
  {
    const uint8_t* const image_bytes = _binary_ringos_test_app_x64_pe64_image_start;
    const size_t image_size
      = static_cast<size_t>(_binary_ringos_test_app_x64_pe64_image_end - _binary_ringos_test_app_x64_pe64_image_start);
    x64_pe64_image_info image_info {};
    const x64_pe64_image_load_status load_status = load_x64_pe64_image(
      image_bytes,
      image_size,
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      storage.user_region,
      X64_USER_REGION_SIZE,
      &INITIAL_WINDOWS_IMPORT_RESOLVER,
      &image_info);

    if (load_status != x64_pe64_image_load_status::OK)
    {
      panic(describe_x64_pe64_image_load_status(load_status));
    }

    *reinterpret_cast<uint64_t*>(storage.user_region + X64_USER_REGION_SIZE - sizeof(uint64_t)) = 0;
    return image_info.entry_point;
  }

  void arm64_initial_user_runtime_platform::write_vector_base(uintptr_t vector_base)
  {
    asm volatile("msr vbar_el1, %0\nisb" : : "r"(vector_base) : "memory");
  }

  void arm64_initial_user_runtime_platform::write_mair(uintptr_t value)
  {
    asm volatile("msr mair_el1, %0\nisb" : : "r"(value) : "memory");
  }

  void arm64_initial_user_runtime_platform::write_tcr(uint64_t value)
  {
    asm volatile("msr tcr_el1, %0\nisb" : : "r"(value) : "memory");
  }

  void arm64_initial_user_runtime_platform::write_ttbr0(uintptr_t value)
  {
    asm volatile("msr ttbr0_el1, %0\nisb" : : "r"(value) : "memory");
  }

  uint64_t arm64_initial_user_runtime_platform::read_system_control() const
  {
    uint64_t value = 0;
    asm volatile("mrs %0, sctlr_el1" : "=r"(value) : : "memory");
    return value;
  }

  void arm64_initial_user_runtime_platform::write_system_control(uint64_t value)
  {
    asm volatile("msr sctlr_el1, %0\nisb" : : "r"(value) : "memory");
  }

  void arm64_initial_user_runtime_platform::invalidate_tlb()
  {
    asm volatile("dsb ishst\ntlbi vmalle1\ndsb ish\nisb" : : : "memory");
  }

  void arm64_initial_user_runtime_platform::enable_mmu()
  {
    const uint64_t tcr_value = TCR_T0SZ_39_BIT_VA | TCR_IRGN0_WRITE_BACK | TCR_ORGN0_WRITE_BACK
      | TCR_SH0_INNER_SHAREABLE | TCR_TG0_4KB | TCR_EPD1_DISABLE;

    write_vector_base(reinterpret_cast<uintptr_t>(arm64_exception_vectors));
    write_mair(MAIR_ATTRIBUTE_NORMAL_WRITE_BACK);
    write_tcr(tcr_value);
    invalidate_tlb();

    const uint64_t system_control
      = read_system_control() | SCTLR_MMU_ENABLE | SCTLR_DATA_CACHE_ENABLE | SCTLR_INSTRUCTION_CACHE_ENABLE;
    write_system_control(system_control);
    m_mmu_enabled = true;
  }

  int32_t arm64_initial_user_runtime_platform::dispatch_x64_syscall(
    void* context, const x64_emulator_state& state, bool* out_should_continue)
  {
    if (context == nullptr || out_should_continue == nullptr)
    {
      return STATUS_INVALID_ARGUMENT;
    }

    user_runtime& runtime = *static_cast<user_runtime*>(context);
    thread* current_thread = runtime.get_current_thread();

    if (current_thread == nullptr)
    {
      return STATUS_BAD_STATE;
    }

    const thread_context user_context {
      state.instruction_pointer,
      static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(x64_general_register::RSP)]),
      static_cast<uintptr_t>(state.flags),
    };
    current_thread->set_user_context(user_context);

    const user_syscall_context syscall_context {
      state.general_registers[static_cast<uint32_t>(x64_general_register::RAX)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::RDI)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::RDX)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::R8)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::R9)],
      static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(x64_general_register::RSP)]),
    };
    const int32_t syscall_status = runtime.dispatch_syscall(syscall_context);
    *out_should_continue = runtime.is_current_thread_runnable();
    return syscall_status;
  }

  void arm64_initial_user_runtime_platform::initialize(initial_user_runtime_bootstrap& bootstrap)
  {
    initialize_process_storage(m_process_storage[0]);
    initialize_translation_tables(m_process_storage[0]);
    const uintptr_t entry_point = initialize_user_image(m_process_storage[0]);

    bootstrap.address_space.arch_root_table = reinterpret_cast<uintptr_t>(&m_process_storage[0].root_table);
    bootstrap.address_space.user_base = USER_REGION_VIRTUAL_ADDRESS;
    bootstrap.address_space.user_size = X64_USER_REGION_SIZE;
    bootstrap.address_space.user_host_base = reinterpret_cast<uintptr_t>(m_process_storage[0].user_region);
    bootstrap.thread_context.instruction_pointer = entry_point;
    bootstrap.thread_context.stack_pointer = X64_USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE - sizeof(uint64_t);
    bootstrap.thread_context.flags = X64_INITIAL_RFLAGS;
    bootstrap.shared_memory_address = USER_REGION_VIRTUAL_ADDRESS;
    bootstrap.shared_memory_size = X64_USER_REGION_SIZE;
  }

  void arm64_initial_user_runtime_platform::prepare_thread_launch(
    const process& initial_process, const thread& initial_thread)
  {
    (void) initial_process;
    (void) initial_thread;

    write_vector_base(reinterpret_cast<uintptr_t>(arm64_exception_vectors));
  }

  void arm64_initial_user_runtime_platform::activate_process_address_space(const process* process_context)
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

    write_ttbr0(root_table);
    invalidate_tlb();

    if (!m_mmu_enabled)
    {
      enable_mmu();
    }
  }

  [[noreturn]] void arm64_initial_user_runtime_platform::enter_user_thread(
    const process& initial_process, const thread& initial_thread)
  {
    activate_process_address_space(&initial_process);

    user_runtime& runtime = get_kernel_user_runtime();
    x64_emulator_state emulated_state {};
    emulated_state.instruction_pointer = initial_thread.get_user_context().instruction_pointer;
    emulated_state.flags = static_cast<uint64_t>(initial_thread.get_user_context().flags);
    emulated_state.general_registers[static_cast<uint32_t>(x64_general_register::RSP)]
      = initial_thread.get_user_context().stack_pointer;
    const x64_emulator_memory memory {
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      m_process_storage[0].user_region,
      X64_USER_REGION_SIZE,
    };
    const x64_emulator_callbacks callbacks {
      &runtime,
      &arm64_initial_user_runtime_platform::dispatch_x64_syscall,
    };
    const x64_emulator_options options {
      x64_emulator_engine::INTERPRETER,
      X64_EMULATOR_INSTRUCTION_BUDGET,
    };
    x64_emulator_result result {};

    debug_log("arm64 x64 emulator runtime ready");

    if (!run_x64_emulator(emulated_state, memory, callbacks, options, &result))
    {
      panic("arm64 failed to launch the x64 emulator backend");
    }

    if (result.completion != x64_emulator_completion::THREAD_EXITED)
    {
      panic(describe_x64_emulator_completion(result.completion));
    }

    arm64_user_thread_exit();
  }
}

void arch_activate_process_address_space(const process* process_context)
{
  g_initial_user_runtime_platform.activate_process_address_space(process_context);
}

extern "C" [[noreturn]] void arm64_unhandled_exception()
{
  panic("arm64 hit an unhandled exception vector");
}

extern "C" bool arm64_handle_syscall(void* frame)
{
  (void) frame;
  panic("arm64 hardware syscall path is disabled while the x64 emulator backend is active");
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

