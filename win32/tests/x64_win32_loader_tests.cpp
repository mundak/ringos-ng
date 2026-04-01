#include "user_space.h"
#include "x64_pe64_image.h"

#include <array>
#include <cstdio>
#include <cstring>

extern "C" const uint8_t _binary_ringos_win32_import_test_app_x64_pe64_image_start[];
extern "C" const uint8_t _binary_ringos_win32_import_test_app_x64_pe64_image_end[];

namespace
{
  constexpr uint16_t PE_DOS_SIGNATURE = 0x5A4D;
  constexpr uint32_t PE_NT_SIGNATURE = 0x00004550;
  constexpr uint32_t PE_IMPORT_DIRECTORY_INDEX = 1;
  constexpr uint64_t PE_IMPORT_BY_ORDINAL64 = 0x8000000000000000ULL;
  constexpr uint64_t PE_IMPORT_RVA_MASK = 0x7FFFFFFFFFFFFFFFULL;

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

  struct [[gnu::packed]] pe_import_descriptor
  {
    uint32_t original_first_thunk;
    uint32_t time_date_stamp;
    uint32_t forwarder_chain;
    uint32_t name_rva;
    uint32_t first_thunk;
  };

  void fail_x64_win32_test(const char* test_name, const char* message)
  {
    std::fprintf(stderr, "FAIL: %s: %s\n", test_name, message);
  }

  bool expect_x64_win32_test(bool condition, const char* test_name, const char* message)
  {
    if (!condition)
    {
      fail_x64_win32_test(test_name, message);
      return false;
    }

    return true;
  }

  bool copy_record(void* destination, size_t size, const uint8_t* image, size_t image_size, size_t offset)
  {
    if (destination == nullptr || image == nullptr || offset > image_size || size > image_size - offset)
    {
      return false;
    }

    std::memcpy(destination, image + offset, size);
    return true;
  }

  bool read_c_string(const uint8_t* image, size_t image_size, uint32_t rva, char* buffer, size_t buffer_size)
  {
    if (image == nullptr || buffer == nullptr || buffer_size == 0 || rva >= image_size)
    {
      return false;
    }

    for (size_t index = 0; index + 1 < buffer_size; ++index)
    {
      const size_t current_offset = static_cast<size_t>(rva) + index;

      if (current_offset >= image_size)
      {
        return false;
      }

      buffer[index] = static_cast<char>(image[current_offset]);

      if (buffer[index] == '\0')
      {
        return true;
      }
    }

    return false;
  }

  bool try_resolve_import_name_rva(uint64_t raw_name_reference, uint32_t image_size, uint32_t* out_name_rva)
  {
    if (out_name_rva == nullptr)
    {
      return false;
    }

    if (raw_name_reference <= 0xFFFFFFFFULL && raw_name_reference < image_size)
    {
      *out_name_rva = static_cast<uint32_t>(raw_name_reference);
      return true;
    }

    if (raw_name_reference < X64_USER_IMAGE_VIRTUAL_ADDRESS)
    {
      return false;
    }

    const uint64_t relative_reference = raw_name_reference - X64_USER_IMAGE_VIRTUAL_ADDRESS;

    if (relative_reference > 0xFFFFFFFFULL || relative_reference >= image_size)
    {
      return false;
    }

    *out_name_rva = static_cast<uint32_t>(relative_reference);
    return true;
  }

  bool try_resolve_import_table_rva(uint32_t raw_rva_or_va, uint32_t image_size, uint32_t* out_rva)
  {
    return try_resolve_import_name_rva(static_cast<uint64_t>(raw_rva_or_va), image_size, out_rva);
  }

  uint32_t expected_syscall_for_import(const char* import_name)
  {
    if (std::strcmp(import_name, "OutputDebugStringA") == 0)
    {
      return static_cast<uint32_t>(STAGE1_SYSCALL_DEBUG_LOG);
    }

    if (std::strcmp(import_name, "ExitProcess") == 0)
    {
      return static_cast<uint32_t>(STAGE1_SYSCALL_THREAD_EXIT);
    }

    return 0;
  }

  bool expect_syscall_stub(
    const uint8_t* loaded_image, size_t loaded_image_size, uint64_t stub_address, uint32_t syscall_number)
  {
    if (stub_address < X64_USER_IMAGE_VIRTUAL_ADDRESS)
    {
      return false;
    }

    const size_t stub_offset = static_cast<size_t>(stub_address - X64_USER_IMAGE_VIRTUAL_ADDRESS);

    if (stub_offset > loaded_image_size || 11 > loaded_image_size - stub_offset)
    {
      return false;
    }

    const uint8_t* stub_bytes = loaded_image + stub_offset;
    return stub_bytes[0] == 0x48 && stub_bytes[1] == 0x89 && stub_bytes[2] == 0xCF && stub_bytes[3] == 0xB8
      && stub_bytes[4] == static_cast<uint8_t>(syscall_number & 0xFFU)
      && stub_bytes[5] == static_cast<uint8_t>((syscall_number >> 8) & 0xFFU)
      && stub_bytes[6] == static_cast<uint8_t>((syscall_number >> 16) & 0xFFU)
      && stub_bytes[7] == static_cast<uint8_t>((syscall_number >> 24) & 0xFFU) && stub_bytes[8] == 0x0F
      && stub_bytes[9] == 0x05 && stub_bytes[10] == 0xC3;
  }

  bool test_loader_rejects_missing_header()
  {
    constexpr std::array<uint8_t, 1> image { 0x00 };
    std::array<uint8_t, X64_USER_REGION_SIZE> loaded_image {};
    x64_pe64_image_info image_info {};
    const x64_pe64_image_load_status status = load_x64_pe64_image(
      image.data(),
      image.size(),
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      loaded_image.data(),
      loaded_image.size(),
      nullptr,
      &image_info);

    return expect_x64_win32_test(
      status == X64_PE64_IMAGE_LOAD_STATUS_MISSING_DOS_HEADER,
      "loader_rejects_missing_header",
      "expected missing DOS header status");
  }

  bool test_loader_resolves_win32_imports()
  {
    const uint8_t* const image_bytes = _binary_ringos_win32_import_test_app_x64_pe64_image_start;
    const size_t image_size = static_cast<size_t>(
      _binary_ringos_win32_import_test_app_x64_pe64_image_end
      - _binary_ringos_win32_import_test_app_x64_pe64_image_start);
    std::array<uint8_t, X64_USER_REGION_SIZE> loaded_image {};
    x64_pe64_image_info image_info {};
    const x64_pe64_image_load_status status = load_x64_pe64_image(
      image_bytes,
      image_size,
      X64_USER_IMAGE_VIRTUAL_ADDRESS,
      loaded_image.data(),
      loaded_image.size(),
      nullptr,
      &image_info);

    if (!expect_x64_win32_test(
          status == X64_PE64_IMAGE_LOAD_STATUS_OK,
          "loader_resolves_win32_imports",
          "expected imported image to load successfully"))
    {
      return false;
    }

    if (!expect_x64_win32_test(
          image_info.import_count == 2, "loader_resolves_win32_imports", "expected two resolved Win32 imports"))
    {
      return false;
    }

    pe_dos_header dos_header {};

    if (
      !copy_record(&dos_header, sizeof(dos_header), loaded_image.data(), loaded_image.size(), 0)
      || dos_header.e_magic != PE_DOS_SIGNATURE)
    {
      return expect_x64_win32_test(false, "loader_resolves_win32_imports", "failed to read DOS header");
    }

    pe_nt_headers64 nt_headers {};

    if (
      !copy_record(
        &nt_headers,
        sizeof(nt_headers),
        loaded_image.data(),
        loaded_image.size(),
        static_cast<size_t>(dos_header.e_lfanew))
      || nt_headers.signature != PE_NT_SIGNATURE)
    {
      return expect_x64_win32_test(false, "loader_resolves_win32_imports", "failed to read NT headers");
    }

    const pe_data_directory& import_directory = nt_headers.optional_header.data_directories[PE_IMPORT_DIRECTORY_INDEX];
    uint32_t descriptor_rva = import_directory.virtual_address;
    size_t verified_imports = 0;

    while (true)
    {
      pe_import_descriptor descriptor {};

      if (!copy_record(&descriptor, sizeof(descriptor), loaded_image.data(), loaded_image.size(), descriptor_rva))
      {
        return expect_x64_win32_test(false, "loader_resolves_win32_imports", "import descriptor was unreadable");
      }

      if (
        descriptor.original_first_thunk == 0 && descriptor.time_date_stamp == 0 && descriptor.forwarder_chain == 0
        && descriptor.name_rva == 0 && descriptor.first_thunk == 0)
      {
        break;
      }

      uint32_t first_thunk_rva = 0;
      uint32_t lookup_rva = 0;

      if (!try_resolve_import_table_rva(descriptor.first_thunk, image_info.image_size, &first_thunk_rva))
      {
        return expect_x64_win32_test(false, "loader_resolves_win32_imports", "first thunk RVA was invalid");
      }

      if (descriptor.original_first_thunk != 0)
      {
        if (!try_resolve_import_table_rva(descriptor.original_first_thunk, image_info.image_size, &lookup_rva))
        {
          return expect_x64_win32_test(false, "loader_resolves_win32_imports", "lookup thunk RVA was invalid");
        }
      }
      else
      {
        lookup_rva = first_thunk_rva;
      }

      uint32_t thunk_index = 0;

      while (true)
      {
        uint64_t lookup_entry = 0;
        uint64_t resolved_entry = 0;

        if (
          !copy_record(
            &lookup_entry,
            sizeof(lookup_entry),
            loaded_image.data(),
            loaded_image.size(),
            lookup_rva + (thunk_index * sizeof(uint64_t)))
          || !copy_record(
            &resolved_entry,
            sizeof(resolved_entry),
            loaded_image.data(),
            loaded_image.size(),
            first_thunk_rva + (thunk_index * sizeof(uint64_t))))
        {
          return expect_x64_win32_test(false, "loader_resolves_win32_imports", "import thunk was unreadable");
        }

        if (lookup_entry == 0)
        {
          break;
        }

        if ((lookup_entry & PE_IMPORT_BY_ORDINAL64) != 0)
        {
          return expect_x64_win32_test(false, "loader_resolves_win32_imports", "did not expect ordinal imports");
        }

        const uint64_t raw_name_reference = lookup_entry & PE_IMPORT_RVA_MASK;
        uint32_t import_name_rva = 0;

        if (!try_resolve_import_name_rva(raw_name_reference, image_info.image_size, &import_name_rva))
        {
          return expect_x64_win32_test(false, "loader_resolves_win32_imports", "import name RVA was invalid");
        }

        char import_name[64] {};

        if (!read_c_string(
              loaded_image.data(),
              loaded_image.size(),
              import_name_rva + sizeof(uint16_t),
              import_name,
              sizeof(import_name)))
        {
          return expect_x64_win32_test(false, "loader_resolves_win32_imports", "import name was unreadable");
        }

        const uint32_t expected_syscall = expected_syscall_for_import(import_name);

        if (!expect_x64_win32_test(
              expected_syscall != 0, "loader_resolves_win32_imports", "unexpected import name in test image"))
        {
          return false;
        }

        if (!expect_x64_win32_test(
              expect_syscall_stub(loaded_image.data(), loaded_image.size(), resolved_entry, expected_syscall),
              "loader_resolves_win32_imports",
              "resolved IAT entry did not point at the expected syscall stub"))
        {
          return false;
        }

        ++verified_imports;
        ++thunk_index;
      }

      descriptor_rva += sizeof(pe_import_descriptor);
    }

    return expect_x64_win32_test(
      verified_imports == 2, "loader_resolves_win32_imports", "expected to verify exactly two imported symbols");
  }
}

int main()
{
  if (!test_loader_rejects_missing_header())
  {
    return 1;
  }

  if (!test_loader_resolves_win32_imports())
  {
    return 1;
  }

  std::puts("PASS: x64 win32 loader unit tests");
  return 0;
}
