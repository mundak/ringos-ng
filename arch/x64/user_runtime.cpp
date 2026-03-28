#include "user_runtime.h"

#include "arch_user_runtime.h"
#include "debug.h"
#include "memory.h"
#include "panic.h"

extern "C" [[noreturn]] void x64_enter_user_thread(
  uintptr_t instruction_pointer, uintptr_t stack_pointer, uintptr_t flags);
extern "C" void x64_syscall_entry();
extern "C" [[noreturn]] void x64_user_thread_exit();
extern "C" const uint8_t _binary_ringos_test_app_x64_pe64_image_start[];
extern "C" const uint8_t _binary_ringos_test_app_x64_pe64_image_end[];

namespace
{
  constexpr size_t PAGE_SIZE = 4096;
  constexpr size_t LOW_IDENTITY_SIZE = 0x400000;
  constexpr size_t LOW_PAGE_TABLE_COUNT = LOW_IDENTITY_SIZE / 0x200000;
  constexpr uintptr_t USER_IMAGE_VIRTUAL_ADDRESS = 0x400000;
  constexpr size_t USER_IMAGE_PAGE_COUNT = 8;
  constexpr uintptr_t USER_STACK_VIRTUAL_ADDRESS = USER_IMAGE_VIRTUAL_ADDRESS + (USER_IMAGE_PAGE_COUNT * PAGE_SIZE);
  constexpr size_t USER_REGION_SIZE = (USER_IMAGE_PAGE_COUNT + 1) * PAGE_SIZE;

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
  constexpr uint16_t PE_DOS_SIGNATURE = 0x5A4D;
  constexpr uint32_t PE_NT_SIGNATURE = 0x00004550;
  constexpr uint16_t PE_MACHINE_X64 = 0x8664;
  constexpr uint16_t PE32_PLUS_MAGIC = 0x20B;
  constexpr uint32_t PE_IMPORT_DIRECTORY_INDEX = 1;
  constexpr uint32_t PE_BASE_RELOCATION_DIRECTORY_INDEX = 5;

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

  static_assert(sizeof(pe_dos_header) == 64);
  static_assert(sizeof(pe_optional_header64) == 240);
  static_assert(sizeof(pe_section_header) == 40);

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

  uint64_t make_table_entry(uintptr_t address, uint64_t flags)
  {
    return (static_cast<uint64_t>(address) & ~0xFFFULL) | flags;
  }

  bool copy_embedded_record(
    void* record, size_t record_size, const uint8_t* image_bytes, size_t image_size, size_t offset)
  {
    if (offset > image_size || record_size > image_size - offset)
    {
      return false;
    }

    memcpy(record, image_bytes + offset, record_size);
    return true;
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
    pe_dos_header dos_header {};

    if (!copy_embedded_record(&dos_header, sizeof(dos_header), image_bytes, image_size, 0))
    {
      panic("x64 test app PE image is missing the DOS header");
    }

    if (dos_header.e_magic != PE_DOS_SIGNATURE || dos_header.e_lfanew < 0)
    {
      panic("x64 test app PE image has an invalid DOS header");
    }

    const size_t nt_offset = static_cast<size_t>(dos_header.e_lfanew);
    pe_nt_headers64 nt_headers {};

    if (!copy_embedded_record(&nt_headers, sizeof(nt_headers), image_bytes, image_size, nt_offset))
    {
      panic("x64 test app PE image is missing the NT headers");
    }

    if (nt_headers.signature != PE_NT_SIGNATURE)
    {
      panic("x64 test app PE image has an invalid NT signature");
    }

    if (nt_headers.file_header.machine != PE_MACHINE_X64)
    {
      panic("x64 test app PE image targets the wrong machine");
    }

    if (nt_headers.file_header.number_of_sections == 0)
    {
      panic("x64 test app PE image does not define any sections");
    }

    if (nt_headers.file_header.size_of_optional_header != sizeof(pe_optional_header64))
    {
      panic("x64 test app PE image has an unsupported optional header size");
    }

    const pe_optional_header64& optional_header = nt_headers.optional_header;

    if (optional_header.magic != PE32_PLUS_MAGIC)
    {
      panic("x64 test app PE image is not PE32+");
    }

    if (optional_header.image_base != USER_IMAGE_VIRTUAL_ADDRESS)
    {
      panic("x64 test app PE image uses an unexpected image base");
    }

    if (optional_header.section_alignment != PAGE_SIZE || optional_header.file_alignment != PAGE_SIZE)
    {
      panic("x64 test app PE image must use 4 KiB section alignment");
    }

    if (optional_header.size_of_image == 0 || optional_header.size_of_image > USER_IMAGE_PAGE_COUNT * PAGE_SIZE)
    {
      panic("x64 test app PE image does not fit in the initial user region");
    }

    if (optional_header.size_of_headers > optional_header.size_of_image || optional_header.size_of_headers > image_size)
    {
      panic("x64 test app PE image headers are out of range");
    }

    if (optional_header.address_of_entry_point >= optional_header.size_of_image)
    {
      panic("x64 test app PE image entry point is out of range");
    }

    if (optional_header.number_of_rva_and_sizes > PE_IMPORT_DIRECTORY_INDEX)
    {
      const pe_data_directory& import_directory = optional_header.data_directories[PE_IMPORT_DIRECTORY_INDEX];

      if (import_directory.virtual_address != 0 || import_directory.size != 0)
      {
        panic("x64 test app PE image unexpectedly imports system libraries");
      }
    }

    if (optional_header.number_of_rva_and_sizes > PE_BASE_RELOCATION_DIRECTORY_INDEX)
    {
      const pe_data_directory& relocation_directory
        = optional_header.data_directories[PE_BASE_RELOCATION_DIRECTORY_INDEX];

      if (relocation_directory.virtual_address != 0 || relocation_directory.size != 0)
      {
        panic("x64 test app PE image unexpectedly requires relocations");
      }
    }

    uint8_t* const loaded_image = &storage.user_image_pages[0][0];
    memcpy(loaded_image, image_bytes, optional_header.size_of_headers);

    const size_t section_headers_offset
      = nt_offset + sizeof(uint32_t) + sizeof(pe_file_header) + nt_headers.file_header.size_of_optional_header;

    for (uint16_t section_index = 0; section_index < nt_headers.file_header.number_of_sections; ++section_index)
    {
      pe_section_header section_header {};
      const size_t current_section_offset
        = section_headers_offset + (static_cast<size_t>(section_index) * sizeof(section_header));

      if (!copy_embedded_record(
            &section_header, sizeof(section_header), image_bytes, image_size, current_section_offset))
      {
        panic("x64 test app PE image section table is truncated");
      }

      const uint32_t mapped_section_size = section_header.virtual_size > section_header.size_of_raw_data
        ? section_header.virtual_size
        : section_header.size_of_raw_data;

      if (
        section_header.virtual_address > optional_header.size_of_image
        || mapped_section_size > optional_header.size_of_image - section_header.virtual_address)
      {
        panic("x64 test app PE image section exceeds the declared image size");
      }

      if (section_header.size_of_raw_data == 0)
      {
        continue;
      }

      if (
        section_header.pointer_to_raw_data > image_size
        || section_header.size_of_raw_data > image_size - section_header.pointer_to_raw_data)
      {
        panic("x64 test app PE image section data is out of range");
      }

      memcpy(
        loaded_image + section_header.virtual_address,
        image_bytes + section_header.pointer_to_raw_data,
        section_header.size_of_raw_data);
    }

    *reinterpret_cast<uint64_t*>(storage.user_stack_page + PAGE_SIZE - sizeof(uint64_t)) = 0;
    return USER_IMAGE_VIRTUAL_ADDRESS + optional_header.address_of_entry_point;
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
    write_cr3(initial_process.get_address_space_info().arch_root_table);

    debug_log("x64 initial user runtime ready");

    x64_enter_user_thread(
      initial_thread.get_user_context().instruction_pointer,
      initial_thread.get_user_context().stack_pointer,
      initial_thread.get_user_context().flags);
  }
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
