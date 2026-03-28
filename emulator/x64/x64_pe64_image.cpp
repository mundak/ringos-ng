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
}

x64_pe64_image_load_status load_x64_pe64_image(
  const uint8_t* image_bytes,
  size_t image_size,
  uintptr_t expected_image_base,
  uint8_t* loaded_image,
  size_t loaded_image_size,
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

  if (optional_header.number_of_rva_and_sizes > PE_IMPORT_DIRECTORY_INDEX)
  {
    const pe_data_directory& import_directory = optional_header.data_directories[PE_IMPORT_DIRECTORY_INDEX];

    if (import_directory.virtual_address != 0 || import_directory.size != 0)
    {
      return x64_pe64_image_load_status::unexpected_imports;
    }
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
  }

  return "x64 PE64 image loader failed with an unknown status";
}
