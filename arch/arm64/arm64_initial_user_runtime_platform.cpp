#include "arm64_initial_user_runtime_platform.h"

#include "debug.h"
#include "klibc/memory.h"
#include "panic.h"
#include "pe_image.h"
#include "x64_emulator.h"
#include "x64_pe64_image.h"
#include "x64_windows_compat.h"

extern "C" const uint8_t arm64_exception_vectors[];
extern "C" const uint8_t _binary_ringos_test_app_image_start[];
extern "C" const uint8_t _binary_ringos_test_app_image_end[];
extern "C" [[noreturn]] void arm64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t spsr);
extern "C" [[noreturn]] void arm64_user_thread_exit();

namespace
{
  constexpr char HEX_DIGITS[] = "0123456789abcdef";
  constexpr size_t PAGE_SIZE = ARM64_USER_RUNTIME_PAGE_SIZE;
  constexpr size_t LARGE_PAGE_SIZE = ARM64_USER_RUNTIME_LARGE_PAGE_SIZE;
  constexpr size_t KERNEL_IDENTITY_SIZE = 0x2000000;
  constexpr uint32_t KERNEL_BLOCK_COUNT = static_cast<uint32_t>(KERNEL_IDENTITY_SIZE / LARGE_PAGE_SIZE);
  constexpr uintptr_t USER_REGION_VIRTUAL_ADDRESS = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr uintptr_t KERNEL_IDENTITY_BASE = 0x40000000;
  constexpr size_t USER_REGION_SIZE = ARM64_USER_RUNTIME_USER_REGION_SIZE;
  constexpr uint32_t USER_BLOCK_INDEX = static_cast<uint32_t>(USER_REGION_VIRTUAL_ADDRESS / LARGE_PAGE_SIZE);
  constexpr uint32_t KERNEL_ROOT_INDEX = static_cast<uint32_t>(KERNEL_IDENTITY_BASE >> 30);
  constexpr uint64_t ARM64_INITIAL_PSTATE = 0;
  constexpr uint64_t X64_INITIAL_RFLAGS = 0x202;
  constexpr size_t WINDOWS_X64_STACK_HOME_SPACE_SIZE = 32;
  constexpr uint64_t X64_EMULATOR_INSTRUCTION_BUDGET = 512;
  constexpr uint64_t ESR_EXCEPTION_CLASS_MASK = 0x3FULL;
  constexpr uint64_t ESR_EXCEPTION_CLASS_SHIFT = 26;
  constexpr uint64_t ESR_EXCEPTION_CLASS_SVC64 = 0x15ULL;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X19_INDEX = 0;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X20_INDEX = 1;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X21_INDEX = 2;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X22_INDEX = 3;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X23_INDEX = 4;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X24_INDEX = 5;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X25_INDEX = 6;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X26_INDEX = 7;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X27_INDEX = 8;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X28_INDEX = 9;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X29_INDEX = 10;
  constexpr uint32_t ARM64_PRESERVED_REGISTER_X30_INDEX = 11;

  constexpr uint64_t TABLE_DESCRIPTOR = 0x3;
  constexpr uint64_t BLOCK_DESCRIPTOR = 0x1;
  constexpr uint64_t ATTRIBUTE_INDEX_NORMAL = 0ULL << 2;
  constexpr uint64_t ACCESS_PERMISSION_USER_RW = 1ULL << 6;
  constexpr uint64_t INNER_SHAREABLE = 3ULL << 8;
  constexpr uint64_t ACCESS_FLAG = 1ULL << 10;

  constexpr uint64_t SCTLR_MMU_ENABLE = 1ULL << 0;
  constexpr uint64_t SCTLR_DATA_CACHE_ENABLE = 1ULL << 2;
  constexpr uint64_t SCTLR_INSTRUCTION_CACHE_ENABLE = 1ULL << 12;
  constexpr uint64_t CPACR_EL1_FPEN_FULL_ACCESS = 3ULL << 20;

  constexpr uint64_t TCR_T0SZ_39_BIT_VA = 25ULL;
  constexpr uint64_t TCR_IRGN0_WRITE_BACK = 1ULL << 8;
  constexpr uint64_t TCR_ORGN0_WRITE_BACK = 1ULL << 10;
  constexpr uint64_t TCR_SH0_INNER_SHAREABLE = 3ULL << 12;
  constexpr uint64_t TCR_TG0_4KB = 0ULL << 14;
  constexpr uint64_t TCR_EPD1_DISABLE = 1ULL << 23;

  constexpr uint64_t MAIR_ATTRIBUTE_NORMAL_WRITE_BACK = 0xFFULL;
  constexpr uint64_t MAIR_ATTRIBUTE_DEVICE_NGNRNE = 0x00ULL << 8;

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
    uint64_t sp_el0;
    uint64_t esr;
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;
  };

  arm64_initial_user_runtime_platform g_initial_user_runtime_platform {};

  void append_hex_nibble(char* buffer, size_t* cursor, size_t capacity, uint8_t value)
  {
    if (*cursor + 1 < capacity)
    {
      buffer[*cursor] = HEX_DIGITS[value & 0xF];
      ++(*cursor);
    }
  }

  void append_hex_u64(char* buffer, size_t* cursor, size_t capacity, uint64_t value)
  {
    for (int32_t shift = 60; shift >= 0; shift -= 4)
    {
      append_hex_nibble(buffer, cursor, capacity, static_cast<uint8_t>((value >> shift) & 0xF));
    }
  }

  void append_literal(char* buffer, size_t* cursor, size_t capacity, const char* value)
  {
    if (value == nullptr)
    {
      return;
    }

    while (*value != '\0' && *cursor + 1 < capacity)
    {
      buffer[*cursor] = *value;
      ++(*cursor);
      ++value;
    }
  }

  void debug_log_x64_emulator_fault(const x64_emulator_result& result)
  {
    char message[80] {};
    size_t cursor = 0;

    append_literal(message, &cursor, sizeof(message), "x64 fault ip=0x");
    append_hex_u64(message, &cursor, sizeof(message), static_cast<uint64_t>(result.fault_address));
    append_literal(message, &cursor, sizeof(message), " opcode=0x");
    append_hex_nibble(message, &cursor, sizeof(message), static_cast<uint8_t>(result.fault_opcode >> 4));
    append_hex_nibble(message, &cursor, sizeof(message), result.fault_opcode);
    message[cursor] = '\0';

    debug_log(message);
  }

  uint64_t make_table_descriptor(uintptr_t address)
  {
    return (static_cast<uint64_t>(address) & ~0xFFFULL) | TABLE_DESCRIPTOR;
  }

  uint64_t make_block_descriptor(uintptr_t address, uint64_t flags)
  {
    return (static_cast<uint64_t>(address) & ~0x1FFFFFULL) | flags | BLOCK_DESCRIPTOR;
  }

  uintptr_t align_up(uintptr_t value, size_t alignment)
  {
    const uintptr_t alignment_mask = static_cast<uintptr_t>(alignment - 1);
    return (value + alignment_mask) & ~alignment_mask;
  }

  uint8_t* get_user_region(arm64_process_storage& storage)
  {
    return reinterpret_cast<uint8_t*>(
      align_up(reinterpret_cast<uintptr_t>(storage.user_region_storage), LARGE_PAGE_SIZE));
  }

  void load_arm64_syscall_frame(arm64_syscall_frame* frame, thread& current_thread)
  {
    if (frame == nullptr)
    {
      return;
    }

    memset(frame, 0, sizeof(*frame));
    const user_thread_resume& resume_state = current_thread.get_resume_state();
    frame->x0 = static_cast<uint64_t>(resume_state.argument0);
    frame->elr = resume_state.instruction_pointer;
    frame->spsr = resume_state.flags;
    frame->sp_el0 = resume_state.stack_pointer;
    const uintptr_t* const preserved_registers = current_thread.get_arch_preserved_registers();
    frame->x19 = preserved_registers[ARM64_PRESERVED_REGISTER_X19_INDEX];
    frame->x20 = preserved_registers[ARM64_PRESERVED_REGISTER_X20_INDEX];
    frame->x21 = preserved_registers[ARM64_PRESERVED_REGISTER_X21_INDEX];
    frame->x22 = preserved_registers[ARM64_PRESERVED_REGISTER_X22_INDEX];
    frame->x23 = preserved_registers[ARM64_PRESERVED_REGISTER_X23_INDEX];
    frame->x24 = preserved_registers[ARM64_PRESERVED_REGISTER_X24_INDEX];
    frame->x25 = preserved_registers[ARM64_PRESERVED_REGISTER_X25_INDEX];
    frame->x26 = preserved_registers[ARM64_PRESERVED_REGISTER_X26_INDEX];
    frame->x27 = preserved_registers[ARM64_PRESERVED_REGISTER_X27_INDEX];
    frame->x28 = preserved_registers[ARM64_PRESERVED_REGISTER_X28_INDEX];
    frame->x29 = preserved_registers[ARM64_PRESERVED_REGISTER_X29_INDEX];
    frame->x30 = resume_state.kind == USER_THREAD_RESUME_KIND_RPC
      ? resume_state.rpc_completion_address
      : preserved_registers[ARM64_PRESERVED_REGISTER_X30_INDEX];
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

  [[noreturn]] void arm64_park_cpu()
  {
    asm volatile("msr daifset, #0xf\nisb" : : : "memory");

    while (true)
    {
      asm volatile("wfe" : : : "memory");
    }
  }

  static_assert((KERNEL_IDENTITY_SIZE % LARGE_PAGE_SIZE) == 0);
  static_assert((USER_REGION_VIRTUAL_ADDRESS % LARGE_PAGE_SIZE) == 0);
  static_assert(USER_BLOCK_INDEX < 512);
  static_assert(KERNEL_ROOT_INDEX < 512);
}

arm64_initial_user_runtime_platform& get_arm64_initial_user_runtime_platform()
{
  return g_initial_user_runtime_platform;
}

void initialize_arm64_platform(void* context, initial_user_runtime_bootstrap& bootstrap)
{
  static_cast<arm64_initial_user_runtime_platform*>(context)->initialize(bootstrap);
}

void prepare_arm64_platform(void* context, const process& initial_process, const thread& initial_thread)
{
  static_cast<arm64_initial_user_runtime_platform*>(context)->prepare_thread_launch(initial_process, initial_thread);
}

[[noreturn]] void enter_arm64_platform(void* context, const process& initial_process, const thread& initial_thread)
{
  static_cast<arm64_initial_user_runtime_platform*>(context)->enter_user_thread(initial_process, initial_thread);
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
  uint8_t* const user_region = get_user_region(storage);
  storage.lower_block_table.entries[USER_BLOCK_INDEX] = make_block_descriptor(
    reinterpret_cast<uintptr_t>(user_region),
    ATTRIBUTE_INDEX_NORMAL | ACCESS_PERMISSION_USER_RW | INNER_SHAREABLE | ACCESS_FLAG);

  for (uint32_t block_index = 0; block_index < KERNEL_BLOCK_COUNT; ++block_index)
  {
    storage.kernel_block_table.entries[block_index] = make_block_descriptor(
      KERNEL_IDENTITY_BASE + (static_cast<uintptr_t>(block_index) * LARGE_PAGE_SIZE),
      ATTRIBUTE_INDEX_NORMAL | INNER_SHAREABLE | ACCESS_FLAG);
  }
}

arm64_user_image_kind arm64_initial_user_runtime_platform::get_user_image_kind() const
{
  return m_user_image_kind;
}

uintptr_t arm64_initial_user_runtime_platform::initialize_arm64_pe_image(
  const uint8_t* image_bytes, size_t image_size, arm64_process_storage& storage)
{
  uint8_t* const user_region = get_user_region(storage);
  const pe_image_load_config load_config {
    .expected_machine = PE_MACHINE_ARM64,
    .expected_image_base = USER_REGION_VIRTUAL_ADDRESS,
    .expected_section_alignment = PAGE_SIZE,
    .expected_file_alignment = PAGE_SIZE,
    .loaded_image_size = USER_REGION_SIZE,
    .allow_imports = false,
    .allow_relocations = false,
  };
  pe_image_load_result load_result {};
  const pe_image_load_status load_status
    = load_pe32_plus_image(image_bytes, image_size, user_region, load_config, &load_result);

  if (load_status != PE_IMAGE_LOAD_STATUS_OK)
  {
    panic(describe_pe_image_load_status(load_status));
  }

  return load_result.entry_point;
}

uintptr_t arm64_initial_user_runtime_platform::initialize_x64_emulator_image(
  const uint8_t* image_bytes, size_t image_size, arm64_process_storage& storage)
{
  uint8_t* const user_region = get_user_region(storage);

  memset(user_region, 0, USER_REGION_SIZE);

  x64_pe64_image_info image_info {};
  const x64_pe64_image_load_status load_status = load_x64_pe64_image(
    image_bytes,
    image_size,
    X64_USER_IMAGE_VIRTUAL_ADDRESS,
    user_region,
    X64_USER_REGION_SIZE,
    &INITIAL_WINDOWS_IMPORT_RESOLVER,
    &image_info);

  if (load_status != X64_PE64_IMAGE_LOAD_STATUS_OK)
  {
    panic(describe_x64_pe64_image_load_status(load_status));
  }

  *reinterpret_cast<uint64_t*>(user_region + X64_USER_REGION_SIZE - sizeof(uint64_t)) = 0;
  return image_info.entry_point;
}

void arm64_initial_user_runtime_platform::populate_native_bootstrap_for_process(
  arm64_process_storage& storage,
  const uint8_t* image_start,
  const uint8_t* image_end,
  address_space& address_space_info,
  thread_context& thread_context_info)
{
  initialize_process_storage(storage);
  initialize_translation_tables(storage);
  uint8_t* const user_region = get_user_region(storage);
  const uintptr_t entry_point
    = initialize_arm64_pe_image(image_start, static_cast<size_t>(image_end - image_start), storage);

  address_space_info.arch_root_table = reinterpret_cast<uintptr_t>(&storage.root_table);
  address_space_info.user_base = USER_REGION_VIRTUAL_ADDRESS;
  address_space_info.user_size = USER_REGION_SIZE;
  address_space_info.user_host_base = reinterpret_cast<uintptr_t>(user_region);

  thread_context_info.instruction_pointer = entry_point;
  thread_context_info.stack_pointer = USER_REGION_VIRTUAL_ADDRESS + USER_REGION_SIZE;
  thread_context_info.flags = ARM64_INITIAL_PSTATE;
  thread_context_info.argument0 = 0;
}

void arm64_initial_user_runtime_platform::populate_x64_emulator_bootstrap(
  arm64_process_storage& storage,
  const uint8_t* image_start,
  const uint8_t* image_end,
  address_space& address_space_info,
  thread_context& thread_context_info)
{
  initialize_process_storage(storage);
  initialize_translation_tables(storage);
  uint8_t* const user_region = get_user_region(storage);
  const uintptr_t entry_point
    = initialize_x64_emulator_image(image_start, static_cast<size_t>(image_end - image_start), storage);

  address_space_info.arch_root_table = reinterpret_cast<uintptr_t>(&storage.root_table);
  address_space_info.user_base = USER_REGION_VIRTUAL_ADDRESS;
  address_space_info.user_size = X64_USER_REGION_SIZE;
  address_space_info.user_host_base = reinterpret_cast<uintptr_t>(user_region);

  thread_context_info.instruction_pointer = entry_point;
  thread_context_info.stack_pointer
    = X64_USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE - sizeof(uint64_t) - WINDOWS_X64_STACK_HOME_SPACE_SIZE;
  thread_context_info.flags = X64_INITIAL_RFLAGS;
  thread_context_info.argument0 = 0;
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

void arm64_initial_user_runtime_platform::enable_fp_simd()
{
  asm volatile("msr cpacr_el1, %0\nisb" : : "r"(CPACR_EL1_FPEN_FULL_ACCESS) : "memory");
}

void arm64_initial_user_runtime_platform::invalidate_tlb()
{
  asm volatile("dsb ishst\ntlbi vmalle1\ndsb ish\nisb" : : : "memory");
}

void arm64_initial_user_runtime_platform::enable_mmu()
{
  const uint64_t tcr_value = TCR_T0SZ_39_BIT_VA | TCR_IRGN0_WRITE_BACK | TCR_ORGN0_WRITE_BACK | TCR_SH0_INNER_SHAREABLE
    | TCR_TG0_4KB | TCR_EPD1_DISABLE;

  write_vector_base(reinterpret_cast<uintptr_t>(arm64_exception_vectors));
  write_mair(MAIR_ATTRIBUTE_NORMAL_WRITE_BACK | MAIR_ATTRIBUTE_DEVICE_NGNRNE);
  write_tcr(tcr_value);
  enable_fp_simd();
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
    static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RSP)]),
    static_cast<uintptr_t>(state.flags),
    static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RDI)]),
  };
  current_thread->set_user_context(user_context);

  const user_syscall_context syscall_context {
    state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RAX)],
    state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RDI)],
    state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RSI)],
    state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RDX)],
    state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_R10)],
    static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RSP)]),
  };

  const int32_t syscall_status = runtime.dispatch_syscall(syscall_context);

  if (runtime.get_current_thread() == current_thread)
  {
    current_thread->prepare_syscall_resume(syscall_status);
  }

  *out_should_continue = runtime.has_runnable_thread();
  return runtime.get_current_thread() != nullptr ? runtime.get_current_thread()->get_pending_syscall_status()
                                                 : syscall_status;
}

void arm64_initial_user_runtime_platform::initialize(initial_user_runtime_bootstrap& bootstrap)
{
  memset(&bootstrap, 0, sizeof(bootstrap));

  uint16_t pe_machine = 0;
  const uint8_t* const test_app_image_start = _binary_ringos_test_app_image_start;
  const size_t test_app_image_size
    = static_cast<size_t>(_binary_ringos_test_app_image_end - _binary_ringos_test_app_image_start);

  if (try_get_pe_machine(test_app_image_start, test_app_image_size, &pe_machine) && pe_machine == PE_MACHINE_ARM64)
  {
    m_user_image_kind = ARM64_USER_IMAGE_KIND_NATIVE_ARM64_PE64;
    bootstrap.process_count = 1;
    bootstrap.initial_process_index = 0;
    populate_native_bootstrap_for_process(
      m_process_storage[0],
      _binary_ringos_test_app_image_start,
      _binary_ringos_test_app_image_end,
      bootstrap.address_space[0],
      bootstrap.thread_context[0]);
    return;
  }

  if (try_get_pe_machine(test_app_image_start, test_app_image_size, &pe_machine) && pe_machine == PE_MACHINE_X64)
  {
    m_user_image_kind = ARM64_USER_IMAGE_KIND_X64_PE64;
    bootstrap.process_count = 1;
    bootstrap.initial_process_index = 0;
    populate_x64_emulator_bootstrap(
      m_process_storage[0],
      _binary_ringos_test_app_image_start,
      _binary_ringos_test_app_image_end,
      bootstrap.address_space[0],
      bootstrap.thread_context[0]);
    return;
  }

  panic("arm64 attached user image has an unsupported signature");
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

  if (m_user_image_kind == ARM64_USER_IMAGE_KIND_NATIVE_ARM64_PE64)
  {
    debug_log("arm64 initial user runtime ready");

    arm64_enter_user_thread(
      initial_thread.get_user_context().instruction_pointer,
      initial_thread.get_user_context().stack_pointer,
      initial_thread.get_user_context().flags);
  }

  if (m_user_image_kind == ARM64_USER_IMAGE_KIND_X64_PE64)
  {
    user_runtime& runtime = get_kernel_user_runtime();
    x64_emulator_state emulated_state {};
    emulated_state.instruction_pointer = initial_thread.get_user_context().instruction_pointer;
    emulated_state.flags = static_cast<uint64_t>(initial_thread.get_user_context().flags);
    emulated_state.general_registers[static_cast<uint32_t>(X64_GENERAL_REGISTER_RSP)]
      = initial_thread.get_user_context().stack_pointer;
    const x64_emulator_memory memory {
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      get_user_region(m_process_storage[0]),
      X64_USER_REGION_SIZE,
    };
    const x64_emulator_callbacks callbacks {
      &runtime,
      &arm64_initial_user_runtime_platform::dispatch_x64_syscall,
    };
    const x64_emulator_options options {
      X64_EMULATOR_ENGINE_INTERPRETER,
      X64_EMULATOR_INSTRUCTION_BUDGET,
    };
    x64_emulator_result result {};

    debug_log("arm64 x64 emulator runtime ready");

    if (!run_x64_emulator(emulated_state, memory, callbacks, options, &result))
    {
      panic("arm64 failed to launch the x64 emulator backend");
    }

    if (result.completion != X64_EMULATOR_COMPLETION_THREAD_EXITED)
    {
      debug_log_x64_emulator_fault(result);
      panic(describe_x64_emulator_completion(result.completion));
    }

    arm64_user_thread_exit();
  }

  panic("arm64 user image kind was not initialized");
}

extern "C" [[noreturn]] void arm64_unhandled_exception()
{
  panic("arm64 hit an unhandled exception vector");
}

extern "C" bool arm64_handle_syscall(void* frame)
{
  if (frame == nullptr)
  {
    panic("arm64 syscall frame was null");
  }

  if (g_initial_user_runtime_platform.get_user_image_kind() == ARM64_USER_IMAGE_KIND_X64_PE64)
  {
    panic("arm64 hardware syscall path is disabled while the x64 emulator backend is active");
  }

  if (g_initial_user_runtime_platform.get_user_image_kind() != ARM64_USER_IMAGE_KIND_NATIVE_ARM64_PE64)
  {
    panic("arm64 syscall path was entered without a native arm64 user image");
  }

  arm64_syscall_frame* const syscall_frame = static_cast<arm64_syscall_frame*>(frame);
  const uint64_t exception_class = (syscall_frame->esr >> ESR_EXCEPTION_CLASS_SHIFT) & ESR_EXCEPTION_CLASS_MASK;

  if (exception_class != ESR_EXCEPTION_CLASS_SVC64)
  {
    panic("arm64 lower-el sync was not an svc64 syscall");
  }

  user_runtime& runtime = get_kernel_user_runtime();
  thread* current_thread = runtime.get_current_thread();

  if (current_thread == nullptr)
  {
    panic("arm64 syscall without current thread");
  }

  const thread_context user_context {
    static_cast<uintptr_t>(syscall_frame->elr),
    static_cast<uintptr_t>(syscall_frame->sp_el0),
    static_cast<uintptr_t>(syscall_frame->spsr),
    static_cast<uintptr_t>(syscall_frame->x0),
  };
  current_thread->set_user_context(user_context);
  uintptr_t* const preserved_registers = current_thread->get_arch_preserved_registers();
  preserved_registers[ARM64_PRESERVED_REGISTER_X19_INDEX] = static_cast<uintptr_t>(syscall_frame->x19);
  preserved_registers[ARM64_PRESERVED_REGISTER_X20_INDEX] = static_cast<uintptr_t>(syscall_frame->x20);
  preserved_registers[ARM64_PRESERVED_REGISTER_X21_INDEX] = static_cast<uintptr_t>(syscall_frame->x21);
  preserved_registers[ARM64_PRESERVED_REGISTER_X22_INDEX] = static_cast<uintptr_t>(syscall_frame->x22);
  preserved_registers[ARM64_PRESERVED_REGISTER_X23_INDEX] = static_cast<uintptr_t>(syscall_frame->x23);
  preserved_registers[ARM64_PRESERVED_REGISTER_X24_INDEX] = static_cast<uintptr_t>(syscall_frame->x24);
  preserved_registers[ARM64_PRESERVED_REGISTER_X25_INDEX] = static_cast<uintptr_t>(syscall_frame->x25);
  preserved_registers[ARM64_PRESERVED_REGISTER_X26_INDEX] = static_cast<uintptr_t>(syscall_frame->x26);
  preserved_registers[ARM64_PRESERVED_REGISTER_X27_INDEX] = static_cast<uintptr_t>(syscall_frame->x27);
  preserved_registers[ARM64_PRESERVED_REGISTER_X28_INDEX] = static_cast<uintptr_t>(syscall_frame->x28);
  preserved_registers[ARM64_PRESERVED_REGISTER_X29_INDEX] = static_cast<uintptr_t>(syscall_frame->x29);
  preserved_registers[ARM64_PRESERVED_REGISTER_X30_INDEX] = static_cast<uintptr_t>(syscall_frame->x30);

  const user_syscall_context syscall_context {
    syscall_frame->x8, syscall_frame->x0, syscall_frame->x1,
    syscall_frame->x2, syscall_frame->x3, static_cast<uintptr_t>(syscall_frame->sp_el0),
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
    resume_thread->prepare_syscall_resume(syscall_status);
  }

  load_arm64_syscall_frame(syscall_frame, *resume_thread);
  return runtime.has_runnable_thread();
}

extern "C" [[noreturn]] void arm64_user_thread_exit()
{
  debug_log("arm64 returned to kernel after user runtime exit");
  arm64_park_cpu();
}
