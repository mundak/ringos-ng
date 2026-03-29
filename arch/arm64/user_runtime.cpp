#include "user_runtime.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"
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
  constexpr size_t PAGE_SIZE = 4096;
  constexpr size_t LARGE_PAGE_SIZE = 0x200000;
  constexpr size_t KERNEL_IDENTITY_SIZE = 0x2000000;
  constexpr uint32_t KERNEL_BLOCK_COUNT = static_cast<uint32_t>(KERNEL_IDENTITY_SIZE / LARGE_PAGE_SIZE);
  constexpr uintptr_t USER_REGION_VIRTUAL_ADDRESS = X64_USER_IMAGE_VIRTUAL_ADDRESS;
  constexpr uintptr_t KERNEL_IDENTITY_BASE = 0x40000000;
  constexpr size_t USER_REGION_SIZE = LARGE_PAGE_SIZE;
  constexpr uint32_t USER_BLOCK_INDEX = static_cast<uint32_t>(USER_REGION_VIRTUAL_ADDRESS / LARGE_PAGE_SIZE);
  constexpr uint32_t KERNEL_ROOT_INDEX = static_cast<uint32_t>(KERNEL_IDENTITY_BASE >> 30);
  constexpr uint64_t ARM64_INITIAL_PSTATE = 0;
  constexpr uint64_t X64_INITIAL_RFLAGS = 0x202;
  constexpr uint64_t X64_EMULATOR_INSTRUCTION_BUDGET = 512;
  constexpr uint16_t PE_DOS_SIGNATURE = 0x5A4D;
  constexpr uint32_t PE_NT_SIGNATURE = 0x00004550;
  constexpr uint16_t PE_MACHINE_ARM64 = 0xAA64;
  constexpr uint16_t PE_MACHINE_X64 = 0x8664;
  constexpr uint16_t PE32_PLUS_MAGIC = 0x20B;
  constexpr uint32_t PE_IMPORT_DIRECTORY_INDEX = 1;
  constexpr uint32_t PE_BASE_RELOCATION_DIRECTORY_INDEX = 5;
  constexpr uint64_t ESR_EXCEPTION_CLASS_MASK = 0x3FULL;
  constexpr uint64_t ESR_EXCEPTION_CLASS_SHIFT = 26;
  constexpr uint64_t ESR_EXCEPTION_CLASS_SVC64 = 0x15ULL;

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
  };

  enum class arm64_user_image_kind : uint32_t
  {
    unknown = 0,
    native_arm64_pe64 = 1,
    x64_pe64 = 2,
  };

  struct [[gnu::packed]] pe_dos_header
  {
    uint16_t e_magic;
    uint8_t unused[58];
    int32_t e_lfanew;
  };

  struct [[gnu::packed]] pe_file_header
  {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
  };

  struct [[gnu::packed]] pe_data_directory
  {
    uint32_t virtual_address;
    uint32_t size;
  };

  struct [[gnu::packed]] pe_optional_header64
  {
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_operating_system_version;
    uint16_t minor_operating_system_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t check_sum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t size_of_stack_reserve;
    uint64_t size_of_stack_commit;
    uint64_t size_of_heap_reserve;
    uint64_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    pe_data_directory data_directories[16];
  };

  struct [[gnu::packed]] pe_nt_headers64
  {
    uint32_t signature;
    pe_file_header file_header;
    pe_optional_header64 optional_header;
  };

  struct [[gnu::packed]] pe_section_header
  {
    uint8_t name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_linenumbers;
    uint16_t number_of_relocations;
    uint16_t number_of_linenumbers;
    uint32_t characteristics;
  };

  class arm64_initial_user_runtime_platform final
  {
  public:
    void initialize(initial_user_runtime_bootstrap& bootstrap);
    void prepare_thread_launch(const process& initial_process, const thread& initial_thread);
    [[noreturn]] void enter_user_thread(const process& initial_process, const thread& initial_thread);
    void activate_process_address_space(const process* process_context);
    arm64_user_image_kind get_user_image_kind() const;

  private:
    static int32_t dispatch_x64_syscall(void* context, const x64_emulator_state& state, bool* out_should_continue);
    uintptr_t initialize_arm64_pe_image(const uint8_t* image_bytes, size_t image_size, arm64_process_storage& storage);
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
    arm64_user_image_kind m_user_image_kind = arm64_user_image_kind::unknown;
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

  bool copy_embedded_record(
    void* record, size_t record_size, const uint8_t* image_bytes, size_t image_size, size_t offset)
  {
    if (record == nullptr || image_bytes == nullptr)
    {
      return false;
    }

    if (offset > image_size || record_size > image_size - offset)
    {
      return false;
    }

    memcpy(record, image_bytes + offset, record_size);
    return true;
  }

  bool try_get_pe_machine(const uint8_t* image_bytes, size_t image_size, uint16_t* out_machine)
  {
    if (out_machine == nullptr)
    {
      return false;
    }

    pe_dos_header dos_header {};

    if (!copy_embedded_record(&dos_header, sizeof(dos_header), image_bytes, image_size, 0))
    {
      return false;
    }

    if (dos_header.e_magic != PE_DOS_SIGNATURE || dos_header.e_lfanew < 0)
    {
      return false;
    }

    pe_nt_headers64 nt_headers {};

    if (!copy_embedded_record(
          &nt_headers, sizeof(nt_headers), image_bytes, image_size, static_cast<size_t>(dos_header.e_lfanew)))
    {
      return false;
    }

    if (nt_headers.signature != PE_NT_SIGNATURE || nt_headers.optional_header.magic != PE32_PLUS_MAGIC)
    {
      return false;
    }

    *out_machine = nt_headers.file_header.machine;
    return true;
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

  arm64_user_image_kind arm64_initial_user_runtime_platform::get_user_image_kind() const
  {
    return m_user_image_kind;
  }

  uintptr_t arm64_initial_user_runtime_platform::initialize_arm64_pe_image(
    const uint8_t* image_bytes, size_t image_size, arm64_process_storage& storage)
  {
    pe_dos_header dos_header {};

    if (!copy_embedded_record(&dos_header, sizeof(dos_header), image_bytes, image_size, 0))
    {
      panic("arm64 PE image is missing the DOS header");
    }

    if (dos_header.e_magic != PE_DOS_SIGNATURE)
    {
      panic("arm64 PE image has an invalid DOS header");
    }

    if (dos_header.e_lfanew < 0)
    {
      panic("arm64 PE image is missing the NT headers");
    }

    const size_t nt_headers_offset = static_cast<size_t>(dos_header.e_lfanew);
    pe_nt_headers64 nt_headers {};

    if (!copy_embedded_record(&nt_headers, sizeof(nt_headers), image_bytes, image_size, nt_headers_offset))
    {
      panic("arm64 PE image is missing the NT headers");
    }

    if (nt_headers.signature != PE_NT_SIGNATURE)
    {
      panic("arm64 PE image has an invalid NT signature");
    }

    if (nt_headers.file_header.machine != PE_MACHINE_ARM64)
    {
      panic("arm64 PE image targets the wrong machine");
    }

    if (nt_headers.file_header.number_of_sections == 0)
    {
      panic("arm64 PE image does not define any sections");
    }

    if (nt_headers.file_header.size_of_optional_header != sizeof(pe_optional_header64))
    {
      panic("arm64 PE image has an unsupported optional header size");
    }

    if (nt_headers.optional_header.magic != PE32_PLUS_MAGIC)
    {
      panic("arm64 PE image is not PE32+");
    }

    if (nt_headers.optional_header.image_base != USER_REGION_VIRTUAL_ADDRESS)
    {
      panic("arm64 PE image uses an unexpected image base");
    }

    if (
      nt_headers.optional_header.section_alignment != PAGE_SIZE
      || nt_headers.optional_header.file_alignment != PAGE_SIZE)
    {
      panic("arm64 PE image uses an unsupported alignment");
    }

    if (nt_headers.optional_header.size_of_image > USER_REGION_SIZE)
    {
      panic("arm64 PE image does not fit in the initial user region");
    }

    if (
      nt_headers.optional_header.size_of_headers > image_size
      || nt_headers.optional_header.size_of_headers > USER_REGION_SIZE)
    {
      panic("arm64 PE image headers are out of range");
    }

    if (nt_headers.optional_header.address_of_entry_point >= nt_headers.optional_header.size_of_image)
    {
      panic("arm64 PE image entry point is out of range");
    }

    const pe_data_directory& import_directory = nt_headers.optional_header.data_directories[PE_IMPORT_DIRECTORY_INDEX];

    if (import_directory.virtual_address != 0 || import_directory.size != 0)
    {
      panic("arm64 PE image unexpectedly imports system libraries");
    }

    const pe_data_directory& relocation_directory
      = nt_headers.optional_header.data_directories[PE_BASE_RELOCATION_DIRECTORY_INDEX];

    if (relocation_directory.virtual_address != 0 || relocation_directory.size != 0)
    {
      panic("arm64 PE image unexpectedly requires relocations");
    }

    const size_t section_headers_offset
      = nt_headers_offset + sizeof(uint32_t) + sizeof(pe_file_header) + nt_headers.file_header.size_of_optional_header;
    const size_t section_headers_size = sizeof(pe_section_header) * nt_headers.file_header.number_of_sections;

    if (section_headers_offset > image_size || section_headers_size > image_size - section_headers_offset)
    {
      panic("arm64 PE image section table is truncated");
    }

    memset(storage.user_region, 0, sizeof(storage.user_region));
    memcpy(storage.user_region, image_bytes, nt_headers.optional_header.size_of_headers);

    for (uint16_t section_index = 0; section_index < nt_headers.file_header.number_of_sections; ++section_index)
    {
      pe_section_header section_header {};

      if (!copy_embedded_record(
            &section_header,
            sizeof(section_header),
            image_bytes,
            image_size,
            section_headers_offset + (static_cast<size_t>(section_index) * sizeof(pe_section_header))))
      {
        panic("arm64 PE image section table is truncated");
      }

      const uint32_t section_memory_size
        = section_header.virtual_size != 0 ? section_header.virtual_size : section_header.size_of_raw_data;

      if (
        section_header.virtual_address > nt_headers.optional_header.size_of_image
        || section_memory_size > nt_headers.optional_header.size_of_image - section_header.virtual_address)
      {
        panic("arm64 PE image section exceeds the declared image size");
      }

      if (section_header.size_of_raw_data > 0)
      {
        if (
          section_header.pointer_to_raw_data > image_size
          || section_header.size_of_raw_data > image_size - section_header.pointer_to_raw_data)
        {
          panic("arm64 PE image section data is out of range");
        }

        memcpy(
          storage.user_region + section_header.virtual_address,
          image_bytes + section_header.pointer_to_raw_data,
          section_header.size_of_raw_data);
      }

      if (section_memory_size > section_header.size_of_raw_data)
      {
        memset(
          storage.user_region + section_header.virtual_address + section_header.size_of_raw_data,
          0,
          section_memory_size - section_header.size_of_raw_data);
      }
    }

    *reinterpret_cast<uint64_t*>(storage.user_region + USER_REGION_SIZE - sizeof(uint64_t)) = 0;
    return USER_REGION_VIRTUAL_ADDRESS + nt_headers.optional_header.address_of_entry_point;
  }

  uintptr_t arm64_initial_user_runtime_platform::initialize_user_image(arm64_process_storage& storage)
  {
    const uint8_t* const image_bytes = _binary_ringos_test_app_image_start;
    const size_t image_size
      = static_cast<size_t>(_binary_ringos_test_app_image_end - _binary_ringos_test_app_image_start);
    uint16_t pe_machine = 0;

    if (try_get_pe_machine(image_bytes, image_size, &pe_machine) && pe_machine == PE_MACHINE_ARM64)
    {
      m_user_image_kind = arm64_user_image_kind::native_arm64_pe64;
      return initialize_arm64_pe_image(image_bytes, image_size, storage);
    }

    if (try_get_pe_machine(image_bytes, image_size, &pe_machine) && pe_machine == PE_MACHINE_X64)
    {
      m_user_image_kind = arm64_user_image_kind::x64_pe64;
      memset(storage.user_region, 0, sizeof(storage.user_region));

      x64_pe64_image_info image_info {};
      const x64_pe64_image_load_status load_status = load_x64_pe64_image(
        image_bytes,
        image_size,
        X64_USER_IMAGE_VIRTUAL_ADDRESS,
        storage.user_region,
        X64_USER_REGION_SIZE,
        &INITIAL_WINDOWS_IMPORT_RESOLVER,
        &image_info);

      if (load_status != x64_pe64_image_load_status::ok)
      {
        panic(describe_x64_pe64_image_load_status(load_status));
      }

      *reinterpret_cast<uint64_t*>(storage.user_region + X64_USER_REGION_SIZE - sizeof(uint64_t)) = 0;
      return image_info.entry_point;
    }

    panic("arm64 attached user image has an unsupported signature");
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
      static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(x64_general_register::rsp)]),
      static_cast<uintptr_t>(state.flags),
    };
    current_thread->set_user_context(user_context);

    const user_syscall_context syscall_context {
      state.general_registers[static_cast<uint32_t>(x64_general_register::rax)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::rdi)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::rdx)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::r8)],
      state.general_registers[static_cast<uint32_t>(x64_general_register::r9)],
      static_cast<uintptr_t>(state.general_registers[static_cast<uint32_t>(x64_general_register::rsp)]),
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
    bootstrap.address_space.user_size
      = m_user_image_kind == arm64_user_image_kind::native_arm64_pe64 ? USER_REGION_SIZE : X64_USER_REGION_SIZE;
    bootstrap.address_space.user_host_base = reinterpret_cast<uintptr_t>(m_process_storage[0].user_region);
    bootstrap.thread_context.instruction_pointer = entry_point;

    if (m_user_image_kind == arm64_user_image_kind::native_arm64_pe64)
    {
      bootstrap.thread_context.stack_pointer = USER_REGION_VIRTUAL_ADDRESS + USER_REGION_SIZE - sizeof(uint64_t);
      bootstrap.thread_context.flags = ARM64_INITIAL_PSTATE;
    }
    else
    {
      bootstrap.thread_context.stack_pointer = X64_USER_STACK_VIRTUAL_ADDRESS + PAGE_SIZE - sizeof(uint64_t);
      bootstrap.thread_context.flags = X64_INITIAL_RFLAGS;
    }

    bootstrap.shared_memory_address = USER_REGION_VIRTUAL_ADDRESS;
    bootstrap.shared_memory_size
      = m_user_image_kind == arm64_user_image_kind::native_arm64_pe64 ? USER_REGION_SIZE : X64_USER_REGION_SIZE;
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

    if (m_user_image_kind == arm64_user_image_kind::native_arm64_pe64)
    {
      debug_log("arm64 initial user runtime ready");

      arm64_enter_user_thread(
        initial_thread.get_user_context().instruction_pointer,
        initial_thread.get_user_context().stack_pointer,
        initial_thread.get_user_context().flags);
    }

    if (m_user_image_kind == arm64_user_image_kind::x64_pe64)
    {
      user_runtime& runtime = get_kernel_user_runtime();
      x64_emulator_state emulated_state {};
      emulated_state.instruction_pointer = initial_thread.get_user_context().instruction_pointer;
      emulated_state.flags = static_cast<uint64_t>(initial_thread.get_user_context().flags);
      emulated_state.general_registers[static_cast<uint32_t>(x64_general_register::rsp)]
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
        x64_emulator_engine::interpreter,
        X64_EMULATOR_INSTRUCTION_BUDGET,
      };
      x64_emulator_result result {};

      debug_log("arm64 x64 emulator runtime ready");

      if (!run_x64_emulator(emulated_state, memory, callbacks, options, &result))
      {
        panic("arm64 failed to launch the x64 emulator backend");
      }

      if (result.completion != x64_emulator_completion::thread_exited)
      {
        panic(describe_x64_emulator_completion(result.completion));
      }

      arm64_user_thread_exit();
    }

    panic("arm64 user image kind was not initialized");
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
  if (frame == nullptr)
  {
    panic("arm64 syscall frame was null");
  }

  if (g_initial_user_runtime_platform.get_user_image_kind() == arm64_user_image_kind::x64_pe64)
  {
    panic("arm64 hardware syscall path is disabled while the x64 emulator backend is active");
  }

  if (g_initial_user_runtime_platform.get_user_image_kind() != arm64_user_image_kind::native_arm64_pe64)
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
  };
  current_thread->set_user_context(user_context);

  const user_syscall_context syscall_context {
    syscall_frame->x8, syscall_frame->x0, syscall_frame->x1,
    syscall_frame->x2, syscall_frame->x3, static_cast<uintptr_t>(syscall_frame->sp_el0),
  };
  const int32_t syscall_status = runtime.dispatch_syscall(syscall_context);
  syscall_frame->x0 = static_cast<uint64_t>(static_cast<int64_t>(syscall_status));
  return runtime.is_current_thread_runnable();
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

