#include "pe_image.h"

#include "memory.h"

namespace
{
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

pe_image_load_status load_pe32_plus_image(
  const uint8_t* image_bytes,
  size_t image_size,
  uint8_t* loaded_image,
  const pe_image_load_config& config,
  pe_image_load_result* out_result)
{
  if (image_bytes == nullptr || loaded_image == nullptr || out_result == nullptr)
  {
    return pe_image_load_status::invalid_argument;
  }

  memset(loaded_image, 0, config.loaded_image_size);
  memset(out_result, 0, sizeof(*out_result));

  pe_dos_header dos_header {};

  if (!copy_embedded_record(&dos_header, sizeof(dos_header), image_bytes, image_size, 0))
  {
    return pe_image_load_status::missing_dos_header;
  }

  if (dos_header.e_magic != PE_DOS_SIGNATURE || dos_header.e_lfanew < 0)
  {
    return pe_image_load_status::invalid_dos_header;
  }

  const size_t nt_headers_offset = static_cast<size_t>(dos_header.e_lfanew);
  pe_nt_headers64 nt_headers {};

  if (!copy_embedded_record(&nt_headers, sizeof(nt_headers), image_bytes, image_size, nt_headers_offset))
  {
    return pe_image_load_status::missing_nt_headers;
  }

  if (nt_headers.signature != PE_NT_SIGNATURE)
  {
    return pe_image_load_status::invalid_nt_signature;
  }

  if (nt_headers.file_header.machine != config.expected_machine)
  {
    return pe_image_load_status::wrong_machine;
  }

  if (nt_headers.file_header.number_of_sections == 0)
  {
    return pe_image_load_status::missing_sections;
  }

  if (nt_headers.file_header.size_of_optional_header != sizeof(pe_optional_header64))
  {
    return pe_image_load_status::unsupported_optional_header;
  }

  const pe_optional_header64& optional_header = nt_headers.optional_header;

  if (optional_header.magic != PE32_PLUS_MAGIC)
  {
    return pe_image_load_status::unsupported_magic;
  }

  if (optional_header.image_base != config.expected_image_base)
  {
    return pe_image_load_status::unexpected_image_base;
  }

  if (
    optional_header.section_alignment != config.expected_section_alignment
    || optional_header.file_alignment != config.expected_file_alignment)
  {
    return pe_image_load_status::unsupported_alignment;
  }

  if (optional_header.size_of_image == 0 || optional_header.size_of_image > config.loaded_image_size)
  {
    return pe_image_load_status::image_too_large;
  }

  if (optional_header.size_of_headers > optional_header.size_of_image || optional_header.size_of_headers > image_size)
  {
    return pe_image_load_status::headers_out_of_range;
  }

  if (optional_header.address_of_entry_point >= optional_header.size_of_image)
  {
    return pe_image_load_status::entry_point_out_of_range;
  }

  pe_data_directory import_directory {};

  if (optional_header.number_of_rva_and_sizes > PE_IMPORT_DIRECTORY_INDEX)
  {
    import_directory = optional_header.data_directories[PE_IMPORT_DIRECTORY_INDEX];

    if ((import_directory.virtual_address == 0) != (import_directory.size == 0))
    {
      return pe_image_load_status::invalid_import_directory;
    }

    if (!config.allow_imports && (import_directory.virtual_address != 0 || import_directory.size != 0))
    {
      return pe_image_load_status::unexpected_imports;
    }
  }

  pe_data_directory relocation_directory {};

  if (optional_header.number_of_rva_and_sizes > PE_BASE_RELOCATION_DIRECTORY_INDEX)
  {
    relocation_directory = optional_header.data_directories[PE_BASE_RELOCATION_DIRECTORY_INDEX];

    if (!config.allow_relocations && (relocation_directory.virtual_address != 0 || relocation_directory.size != 0))
    {
      return pe_image_load_status::unexpected_relocations;
    }
  }

  const size_t section_headers_offset
    = nt_headers_offset + sizeof(uint32_t) + sizeof(pe_file_header) + nt_headers.file_header.size_of_optional_header;
  const size_t section_headers_size = sizeof(pe_section_header) * nt_headers.file_header.number_of_sections;

  if (section_headers_offset > image_size || section_headers_size > image_size - section_headers_offset)
  {
    return pe_image_load_status::truncated_section_table;
  }

  memcpy(loaded_image, image_bytes, optional_header.size_of_headers);

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
      return pe_image_load_status::truncated_section_table;
    }

    const uint32_t section_memory_size
      = section_header.virtual_size != 0 ? section_header.virtual_size : section_header.size_of_raw_data;

    if (
      section_header.virtual_address > optional_header.size_of_image
      || section_memory_size > optional_header.size_of_image - section_header.virtual_address)
    {
      return pe_image_load_status::section_out_of_range;
    }

    if (section_header.size_of_raw_data > 0)
    {
      if (
        section_header.pointer_to_raw_data > image_size
        || section_header.size_of_raw_data > image_size - section_header.pointer_to_raw_data)
      {
        return pe_image_load_status::section_data_out_of_range;
      }

      memcpy(
        loaded_image + section_header.virtual_address,
        image_bytes + section_header.pointer_to_raw_data,
        section_header.size_of_raw_data);
    }

    if (section_memory_size > section_header.size_of_raw_data)
    {
      memset(
        loaded_image + section_header.virtual_address + section_header.size_of_raw_data,
        0,
        section_memory_size - section_header.size_of_raw_data);
    }
  }

  out_result->entry_point = config.expected_image_base + optional_header.address_of_entry_point;
  out_result->image_size = optional_header.size_of_image;
  out_result->import_directory = import_directory;
  return pe_image_load_status::ok;
}

const char* describe_pe_image_load_status(pe_image_load_status status)
{
  switch (status)
  {
  case pe_image_load_status::ok:
    return "PE image loaded successfully";
  case pe_image_load_status::invalid_argument:
    return "PE image loader received an invalid argument";
  case pe_image_load_status::missing_dos_header:
    return "PE image is missing the DOS header";
  case pe_image_load_status::invalid_dos_header:
    return "PE image has an invalid DOS header";
  case pe_image_load_status::missing_nt_headers:
    return "PE image is missing the NT headers";
  case pe_image_load_status::invalid_nt_signature:
    return "PE image has an invalid NT signature";
  case pe_image_load_status::wrong_machine:
    return "PE image targets the wrong machine";
  case pe_image_load_status::missing_sections:
    return "PE image does not define any sections";
  case pe_image_load_status::unsupported_optional_header:
    return "PE image has an unsupported optional header size";
  case pe_image_load_status::unsupported_magic:
    return "PE image is not PE32+";
  case pe_image_load_status::unexpected_image_base:
    return "PE image uses an unexpected image base";
  case pe_image_load_status::unsupported_alignment:
    return "PE image uses an unsupported alignment";
  case pe_image_load_status::image_too_large:
    return "PE image does not fit in the initial user region";
  case pe_image_load_status::headers_out_of_range:
    return "PE image headers are out of range";
  case pe_image_load_status::entry_point_out_of_range:
    return "PE image entry point is out of range";
  case pe_image_load_status::invalid_import_directory:
    return "PE image has an invalid import directory";
  case pe_image_load_status::unexpected_imports:
    return "PE image unexpectedly imports system libraries";
  case pe_image_load_status::unexpected_relocations:
    return "PE image unexpectedly requires relocations";
  case pe_image_load_status::truncated_section_table:
    return "PE image section table is truncated";
  case pe_image_load_status::section_out_of_range:
    return "PE image section exceeds the declared image size";
  case pe_image_load_status::section_data_out_of_range:
    return "PE image section data is out of range";
  }

  return "PE image loader failed with an unknown status";
}
