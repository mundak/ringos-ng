#include "x64_pe64_image.h"

#include "memory.h"

namespace
{
  constexpr uint16_t PE_DOS_SIGNATURE = 0x5A4D;
  constexpr uint32_t PE_NT_SIGNATURE = 0x00004550;
  constexpr uint16_t PE_MACHINE_X64 = 0x8664;
  constexpr uint16_t PE32_PLUS_MAGIC = 0x20B;
  constexpr uint32_t PE_IMPORT_DIRECTORY_INDEX = 1;
  constexpr uint32_t PE_BASE_RELOCATION_DIRECTORY_INDEX = 5;
  constexpr uint64_t PE_IMPORT_BY_ORDINAL_FLAG64 = 1ULL << 63;
  constexpr size_t X64_WINDOWS_IMPORT_STUB_SIZE = 8;

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

  struct [[gnu::packed]] pe_import_descriptor
  {
    uint32_t original_first_thunk;
    uint32_t time_date_stamp;
    uint32_t forwarder_chain;
    uint32_t name;
    uint32_t first_thunk;
  };

  struct [[gnu::packed]] pe_import_by_name
  {
    uint16_t hint;
    char name[1];
  };

  static_assert(sizeof(pe_dos_header) == 64);
  static_assert(sizeof(pe_optional_header64) == 240);
  static_assert(sizeof(pe_section_header) == 40);
  static_assert(sizeof(pe_import_descriptor) == 20);

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

  size_t align_up(size_t value, size_t alignment)
  {
    if (alignment == 0)
    {
      return value;
    }

    const size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
  }

  bool copy_loaded_record(
    void* record, size_t record_size, const uint8_t* loaded_image, uint32_t loaded_image_size, uint32_t rva)
  {
    if (record == nullptr || loaded_image == nullptr)
    {
      return false;
    }

    if (rva > loaded_image_size || record_size > loaded_image_size - rva)
    {
      return false;
    }

    memcpy(record, loaded_image + rva, record_size);
    return true;
  }

  uint8_t* translate_loaded_rva(uint8_t* loaded_image, uint32_t loaded_image_size, uint32_t rva, size_t length)
  {
    if (loaded_image == nullptr)
    {
      return nullptr;
    }

    if (rva > loaded_image_size || length > loaded_image_size - rva)
    {
      return nullptr;
    }

    return loaded_image + rva;
  }

  const uint8_t* translate_loaded_rva(
    const uint8_t* loaded_image, uint32_t loaded_image_size, uint32_t rva, size_t length)
  {
    if (loaded_image == nullptr)
    {
      return nullptr;
    }

    if (rva > loaded_image_size || length > loaded_image_size - rva)
    {
      return nullptr;
    }

    return loaded_image + rva;
  }

  bool read_loaded_ascii_string(
    const uint8_t* loaded_image, uint32_t loaded_image_size, uint32_t rva, char* buffer, size_t buffer_size)
  {
    if (loaded_image == nullptr || buffer == nullptr || buffer_size == 0)
    {
      return false;
    }

    for (size_t index = 0; index < buffer_size; ++index)
    {
      const uint8_t* current_character = translate_loaded_rva(loaded_image, loaded_image_size, rva + index, 1);

      if (current_character == nullptr)
      {
        return false;
      }

      buffer[index] = static_cast<char>(*current_character);

      if (buffer[index] == '\0')
      {
        return true;
      }
    }

    buffer[buffer_size - 1] = '\0';
    return false;
  }

  bool is_empty_import_descriptor(const pe_import_descriptor& descriptor)
  {
    return descriptor.original_first_thunk == 0 && descriptor.time_date_stamp == 0 && descriptor.forwarder_chain == 0
      && descriptor.name == 0 && descriptor.first_thunk == 0;
  }

  void write_x64_windows_import_stub(uint8_t* destination, uint32_t syscall_number)
  {
    destination[0] = 0xB8;
    memcpy(destination + 1, &syscall_number, sizeof(syscall_number));
    destination[5] = 0x0F;
    destination[6] = 0x05;
    destination[7] = 0xC3;
  }
}

x64_pe64_image_load_status load_x64_pe64_image(
  const uint8_t* image_bytes,
  size_t image_size,
  uintptr_t expected_image_base,
  uint8_t* loaded_image,
  size_t loaded_image_size,
  const x64_pe64_import_resolver* import_resolver,
  x64_pe64_image_info* out_image_info)
{
  if (image_bytes == nullptr || loaded_image == nullptr || out_image_info == nullptr)
  {
    return x64_pe64_image_load_status::invalid_argument;
  }

  pe_dos_header dos_header {};

  if (!copy_embedded_record(&dos_header, sizeof(dos_header), image_bytes, image_size, 0))
  {
    return x64_pe64_image_load_status::missing_dos_header;
  }

  if (dos_header.e_magic != PE_DOS_SIGNATURE || dos_header.e_lfanew < 0)
  {
    return x64_pe64_image_load_status::invalid_dos_header;
  }

  const size_t nt_offset = static_cast<size_t>(dos_header.e_lfanew);
  pe_nt_headers64 nt_headers {};

  if (!copy_embedded_record(&nt_headers, sizeof(nt_headers), image_bytes, image_size, nt_offset))
  {
    return x64_pe64_image_load_status::missing_nt_headers;
  }

  if (nt_headers.signature != PE_NT_SIGNATURE)
  {
    return x64_pe64_image_load_status::invalid_nt_signature;
  }

  if (nt_headers.file_header.machine != PE_MACHINE_X64)
  {
    return x64_pe64_image_load_status::wrong_machine;
  }

  if (nt_headers.file_header.number_of_sections == 0)
  {
    return x64_pe64_image_load_status::missing_sections;
  }

  if (nt_headers.file_header.size_of_optional_header != sizeof(pe_optional_header64))
  {
    return x64_pe64_image_load_status::unsupported_optional_header;
  }

  const pe_optional_header64& optional_header = nt_headers.optional_header;
  const uint32_t loaded_image_size32 = static_cast<uint32_t>(loaded_image_size);

  if (optional_header.magic != PE32_PLUS_MAGIC)
  {
    return x64_pe64_image_load_status::unsupported_magic;
  }

  if (optional_header.image_base != expected_image_base)
  {
    return x64_pe64_image_load_status::unexpected_image_base;
  }

  if (
    optional_header.section_alignment != X64_USER_IMAGE_PAGE_SIZE
    || optional_header.file_alignment != X64_USER_IMAGE_PAGE_SIZE)
  {
    return x64_pe64_image_load_status::unsupported_alignment;
  }

  if (optional_header.size_of_image == 0 || optional_header.size_of_image > loaded_image_size)
  {
    return x64_pe64_image_load_status::image_too_large;
  }

  if (optional_header.size_of_headers > optional_header.size_of_image || optional_header.size_of_headers > image_size)
  {
    return x64_pe64_image_load_status::headers_out_of_range;
  }

  if (optional_header.address_of_entry_point >= optional_header.size_of_image)
  {
    return x64_pe64_image_load_status::entry_point_out_of_range;
  }

  if (optional_header.number_of_rva_and_sizes > PE_BASE_RELOCATION_DIRECTORY_INDEX)
  {
    const pe_data_directory& relocation_directory
      = optional_header.data_directories[PE_BASE_RELOCATION_DIRECTORY_INDEX];

    if (relocation_directory.virtual_address != 0 || relocation_directory.size != 0)
    {
      return x64_pe64_image_load_status::unexpected_relocations;
    }
  }

  const pe_data_directory* import_directory = nullptr;

  if (optional_header.number_of_rva_and_sizes > PE_IMPORT_DIRECTORY_INDEX)
  {
    import_directory = &optional_header.data_directories[PE_IMPORT_DIRECTORY_INDEX];

    if ((import_directory->virtual_address == 0) != (import_directory->size == 0))
    {
      return x64_pe64_image_load_status::invalid_import_directory;
    }
  }

  memcpy(loaded_image, image_bytes, optional_header.size_of_headers);

  const size_t section_headers_offset
    = nt_offset + sizeof(uint32_t) + sizeof(pe_file_header) + nt_headers.file_header.size_of_optional_header;

  for (uint16_t section_index = 0; section_index < nt_headers.file_header.number_of_sections; ++section_index)
  {
    pe_section_header section_header {};
    const size_t current_section_offset
      = section_headers_offset + (static_cast<size_t>(section_index) * sizeof(section_header));

    if (!copy_embedded_record(&section_header, sizeof(section_header), image_bytes, image_size, current_section_offset))
    {
      return x64_pe64_image_load_status::truncated_section_table;
    }

    const uint32_t mapped_section_size = section_header.virtual_size > section_header.size_of_raw_data
      ? section_header.virtual_size
      : section_header.size_of_raw_data;

    if (
      section_header.virtual_address > optional_header.size_of_image
      || mapped_section_size > optional_header.size_of_image - section_header.virtual_address)
    {
      return x64_pe64_image_load_status::section_out_of_range;
    }

    if (section_header.size_of_raw_data == 0)
    {
      continue;
    }

    if (
      section_header.pointer_to_raw_data > image_size
      || section_header.size_of_raw_data > image_size - section_header.pointer_to_raw_data)
    {
      return x64_pe64_image_load_status::section_data_out_of_range;
    }

    memcpy(
      loaded_image + section_header.virtual_address,
      image_bytes + section_header.pointer_to_raw_data,
      section_header.size_of_raw_data);
  }

  if (import_directory != nullptr && import_directory->virtual_address != 0)
  {
    if (import_resolver == nullptr || import_resolver->resolve_import == nullptr)
    {
      return x64_pe64_image_load_status::unexpected_imports;
    }

    if (
      import_directory->virtual_address > optional_header.size_of_image
      || import_directory->size > optional_header.size_of_image - import_directory->virtual_address)
    {
      return x64_pe64_image_load_status::import_table_out_of_range;
    }

    uint32_t stub_rva = static_cast<uint32_t>(align_up(optional_header.size_of_image, sizeof(uint64_t)));

    if (stub_rva > loaded_image_size32)
    {
      return x64_pe64_image_load_status::import_stub_out_of_range;
    }

    bool saw_terminator = false;
    const uint32_t descriptor_limit = import_directory->virtual_address + import_directory->size;

    for (uint32_t descriptor_rva = import_directory->virtual_address;
         descriptor_rva + sizeof(pe_import_descriptor) <= descriptor_limit;
         descriptor_rva += sizeof(pe_import_descriptor))
    {
      pe_import_descriptor descriptor {};

      if (!copy_loaded_record(
            &descriptor, sizeof(descriptor), loaded_image, optional_header.size_of_image, descriptor_rva))
      {
        return x64_pe64_image_load_status::import_table_out_of_range;
      }

      if (is_empty_import_descriptor(descriptor))
      {
        saw_terminator = true;
        break;
      }

      const uint32_t lookup_table_rva
        = descriptor.original_first_thunk != 0 ? descriptor.original_first_thunk : descriptor.first_thunk;
      char dll_name[64] {};

      if (!read_loaded_ascii_string(
            loaded_image, optional_header.size_of_image, descriptor.name, dll_name, sizeof(dll_name)))
      {
        return x64_pe64_image_load_status::import_name_out_of_range;
      }

      for (uint32_t thunk_index = 0;; ++thunk_index)
      {
        const uint32_t lookup_entry_rva = lookup_table_rva + (thunk_index * sizeof(uint64_t));
        const uint32_t address_entry_rva = descriptor.first_thunk + (thunk_index * sizeof(uint64_t));
        uint64_t lookup_entry = 0;

        if (!copy_loaded_record(
              &lookup_entry, sizeof(lookup_entry), loaded_image, optional_header.size_of_image, lookup_entry_rva))
        {
          return x64_pe64_image_load_status::import_table_out_of_range;
        }

        uint8_t* address_entry
          = translate_loaded_rva(loaded_image, optional_header.size_of_image, address_entry_rva, sizeof(uint64_t));

        if (address_entry == nullptr)
        {
          return x64_pe64_image_load_status::import_table_out_of_range;
        }

        if (lookup_entry == 0)
        {
          break;
        }

        if ((lookup_entry & PE_IMPORT_BY_ORDINAL_FLAG64) != 0)
        {
          return x64_pe64_image_load_status::unsupported_import_ordinal;
        }

        if ((lookup_entry & 0xFFFFFFFF00000000ULL) != 0)
        {
          return x64_pe64_image_load_status::import_table_out_of_range;
        }

        const uint32_t import_name_rva = static_cast<uint32_t>(lookup_entry) + sizeof(uint16_t);
        char function_name[64] {};

        if (!read_loaded_ascii_string(
              loaded_image, optional_header.size_of_image, import_name_rva, function_name, sizeof(function_name)))
        {
          return x64_pe64_image_load_status::import_name_out_of_range;
        }

        uint32_t syscall_number = 0;

        if (!import_resolver->resolve_import(import_resolver->context, dll_name, function_name, &syscall_number))
        {
          return x64_pe64_image_load_status::unsupported_import;
        }

        uint8_t* stub_bytes
          = translate_loaded_rva(loaded_image, loaded_image_size32, stub_rva, X64_WINDOWS_IMPORT_STUB_SIZE);

        if (stub_bytes == nullptr)
        {
          return x64_pe64_image_load_status::import_stub_out_of_range;
        }

        write_x64_windows_import_stub(stub_bytes, syscall_number);
        const uint64_t stub_virtual_address = expected_image_base + stub_rva;
        memcpy(address_entry, &stub_virtual_address, sizeof(stub_virtual_address));
        stub_rva += X64_WINDOWS_IMPORT_STUB_SIZE;
      }
    }

    if (!saw_terminator)
    {
      return x64_pe64_image_load_status::invalid_import_directory;
    }
  }

  out_image_info->entry_point = expected_image_base + optional_header.address_of_entry_point;
  out_image_info->image_size = optional_header.size_of_image;
  return x64_pe64_image_load_status::ok;
}

const char* describe_x64_pe64_image_load_status(x64_pe64_image_load_status status)
{
  switch (status)
  {
  case x64_pe64_image_load_status::ok:
    return "x64 PE64 image loaded successfully";
  case x64_pe64_image_load_status::invalid_argument:
    return "x64 PE64 image loader received an invalid argument";
  case x64_pe64_image_load_status::missing_dos_header:
    return "x64 PE64 image is missing the DOS header";
  case x64_pe64_image_load_status::invalid_dos_header:
    return "x64 PE64 image has an invalid DOS header";
  case x64_pe64_image_load_status::missing_nt_headers:
    return "x64 PE64 image is missing the NT headers";
  case x64_pe64_image_load_status::invalid_nt_signature:
    return "x64 PE64 image has an invalid NT signature";
  case x64_pe64_image_load_status::wrong_machine:
    return "x64 PE64 image targets the wrong machine";
  case x64_pe64_image_load_status::missing_sections:
    return "x64 PE64 image does not define any sections";
  case x64_pe64_image_load_status::unsupported_optional_header:
    return "x64 PE64 image has an unsupported optional header size";
  case x64_pe64_image_load_status::unsupported_magic:
    return "x64 PE64 image is not PE32+";
  case x64_pe64_image_load_status::unexpected_image_base:
    return "x64 PE64 image uses an unexpected image base";
  case x64_pe64_image_load_status::unsupported_alignment:
    return "x64 PE64 image must use 4 KiB section alignment";
  case x64_pe64_image_load_status::image_too_large:
    return "x64 PE64 image does not fit in the initial user region";
  case x64_pe64_image_load_status::headers_out_of_range:
    return "x64 PE64 image headers are out of range";
  case x64_pe64_image_load_status::entry_point_out_of_range:
    return "x64 PE64 image entry point is out of range";
  case x64_pe64_image_load_status::invalid_import_directory:
    return "x64 PE64 image has an invalid import directory";
  case x64_pe64_image_load_status::unexpected_imports:
    return "x64 PE64 image unexpectedly imports system libraries";
  case x64_pe64_image_load_status::unexpected_relocations:
    return "x64 PE64 image unexpectedly requires relocations";
  case x64_pe64_image_load_status::truncated_section_table:
    return "x64 PE64 image section table is truncated";
  case x64_pe64_image_load_status::section_out_of_range:
    return "x64 PE64 image section exceeds the declared image size";
  case x64_pe64_image_load_status::section_data_out_of_range:
    return "x64 PE64 image section data is out of range";
  case x64_pe64_image_load_status::import_table_out_of_range:
    return "x64 PE64 image import table is out of range";
  case x64_pe64_image_load_status::import_name_out_of_range:
    return "x64 PE64 image import name is out of range";
  case x64_pe64_image_load_status::unsupported_import:
    return "x64 PE64 image imports an unsupported Windows symbol";
  case x64_pe64_image_load_status::unsupported_import_ordinal:
    return "x64 PE64 image imports by ordinal, which is not supported yet";
  case x64_pe64_image_load_status::import_stub_out_of_range:
    return "x64 PE64 image import thunks do not fit in the initial user region";
  }

  return "x64 PE64 image loader failed with an unknown status";
}
