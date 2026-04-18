#include "x64_pe64_image.h"

#include "klibc/memory.h"
#include "pe_image.h"

namespace
{
  constexpr uint64_t PE_IMPORT_BY_ORDINAL_MASK64 = 1ULL << 63;

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

  static_assert(sizeof(pe_import_descriptor) == 20);

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

  x64_pe64_image_load_status map_common_load_status(pe_image_load_status status)
  {
    switch (status)
    {
    case PE_IMAGE_LOAD_STATUS_OK:
      return X64_PE64_IMAGE_LOAD_STATUS_OK;
    case PE_IMAGE_LOAD_STATUS_INVALID_ARGUMENT:
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_ARGUMENT;
    case PE_IMAGE_LOAD_STATUS_MISSING_DOS_HEADER:
      return X64_PE64_IMAGE_LOAD_STATUS_MISSING_DOS_HEADER;
    case PE_IMAGE_LOAD_STATUS_INVALID_DOS_HEADER:
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_DOS_HEADER;
    case PE_IMAGE_LOAD_STATUS_MISSING_NT_HEADERS:
      return X64_PE64_IMAGE_LOAD_STATUS_MISSING_NT_HEADERS;
    case PE_IMAGE_LOAD_STATUS_INVALID_NT_SIGNATURE:
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_NT_SIGNATURE;
    case PE_IMAGE_LOAD_STATUS_WRONG_MACHINE:
      return X64_PE64_IMAGE_LOAD_STATUS_WRONG_MACHINE;
    case PE_IMAGE_LOAD_STATUS_MISSING_SECTIONS:
      return X64_PE64_IMAGE_LOAD_STATUS_MISSING_SECTIONS;
    case PE_IMAGE_LOAD_STATUS_UNSUPPORTED_OPTIONAL_HEADER:
      return X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_OPTIONAL_HEADER;
    case PE_IMAGE_LOAD_STATUS_UNSUPPORTED_MAGIC:
      return X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_MAGIC;
    case PE_IMAGE_LOAD_STATUS_UNEXPECTED_IMAGE_BASE:
      return X64_PE64_IMAGE_LOAD_STATUS_UNEXPECTED_IMAGE_BASE;
    case PE_IMAGE_LOAD_STATUS_UNSUPPORTED_ALIGNMENT:
      return X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_ALIGNMENT;
    case PE_IMAGE_LOAD_STATUS_IMAGE_TOO_LARGE:
      return X64_PE64_IMAGE_LOAD_STATUS_IMAGE_TOO_LARGE;
    case PE_IMAGE_LOAD_STATUS_HEADERS_OUT_OF_RANGE:
      return X64_PE64_IMAGE_LOAD_STATUS_HEADERS_OUT_OF_RANGE;
    case PE_IMAGE_LOAD_STATUS_ENTRY_POINT_OUT_OF_RANGE:
      return X64_PE64_IMAGE_LOAD_STATUS_ENTRY_POINT_OUT_OF_RANGE;
    case PE_IMAGE_LOAD_STATUS_INVALID_IMPORT_DIRECTORY:
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_IMPORT_DIRECTORY;
    case PE_IMAGE_LOAD_STATUS_UNEXPECTED_IMPORTS:
      return X64_PE64_IMAGE_LOAD_STATUS_UNEXPECTED_IMPORTS;
    case PE_IMAGE_LOAD_STATUS_UNEXPECTED_RELOCATIONS:
      return X64_PE64_IMAGE_LOAD_STATUS_UNEXPECTED_RELOCATIONS;
    case PE_IMAGE_LOAD_STATUS_TRUNCATED_SECTION_TABLE:
      return X64_PE64_IMAGE_LOAD_STATUS_TRUNCATED_SECTION_TABLE;
    case PE_IMAGE_LOAD_STATUS_SECTION_OUT_OF_RANGE:
      return X64_PE64_IMAGE_LOAD_STATUS_SECTION_OUT_OF_RANGE;
    case PE_IMAGE_LOAD_STATUS_SECTION_DATA_OUT_OF_RANGE:
      return X64_PE64_IMAGE_LOAD_STATUS_SECTION_DATA_OUT_OF_RANGE;
    }

    return X64_PE64_IMAGE_LOAD_STATUS_INVALID_ARGUMENT;
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

  bool try_resolve_import_name_rva(
    uint64_t raw_name_reference, uintptr_t expected_image_base, uint32_t image_size, uint32_t* out_name_rva)
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

    if (raw_name_reference < expected_image_base)
    {
      return false;
    }

    const uint64_t relative_reference = raw_name_reference - expected_image_base;

    if (relative_reference > 0xFFFFFFFFULL || relative_reference >= image_size)
    {
      return false;
    }

    *out_name_rva = static_cast<uint32_t>(relative_reference);
    return true;
  }

  bool try_resolve_import_table_rva(
    uint32_t raw_rva_or_va, uintptr_t expected_image_base, uint32_t image_size, uint32_t* out_rva)
  {
    return try_resolve_import_name_rva(static_cast<uint64_t>(raw_rva_or_va), expected_image_base, image_size, out_rva);
  }

  bool is_empty_import_descriptor(const pe_import_descriptor& descriptor)
  {
    return descriptor.original_first_thunk == 0 && descriptor.time_date_stamp == 0 && descriptor.forwarder_chain == 0
      && descriptor.name == 0 && descriptor.first_thunk == 0;
  }

  x64_pe64_image_load_status resolve_x64_import(
    const x64_pe64_import_resolver* import_resolver,
    const char* dll_name,
    const char* function_name,
    uintptr_t expected_image_base,
    uint8_t* loaded_image,
    uint32_t loaded_image_size,
    size_t* inout_next_stub_offset,
    uint64_t* out_function_address)
  {
    if (loaded_image == nullptr || inout_next_stub_offset == nullptr || out_function_address == nullptr)
    {
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_ARGUMENT;
    }

    if (import_resolver == nullptr || import_resolver->resolve_import == nullptr)
    {
      return X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_IMPORT;
    }

    const x64_pe64_import_resolution_status resolution_status = import_resolver->resolve_import(
      import_resolver->context,
      dll_name,
      function_name,
      expected_image_base,
      loaded_image,
      loaded_image_size,
      inout_next_stub_offset,
      out_function_address);

    switch (resolution_status)
    {
    case X64_PE64_IMPORT_RESOLUTION_STATUS_OK:
      return X64_PE64_IMAGE_LOAD_STATUS_OK;
    case X64_PE64_IMPORT_RESOLUTION_STATUS_INVALID_ARGUMENT:
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_IMPORT_DIRECTORY;
    case X64_PE64_IMPORT_RESOLUTION_STATUS_UNSUPPORTED_IMPORT:
      return X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_IMPORT;
    case X64_PE64_IMPORT_RESOLUTION_STATUS_IMPORT_STUB_OUT_OF_RANGE:
      return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_STUB_OUT_OF_RANGE;
    }

    return X64_PE64_IMAGE_LOAD_STATUS_INVALID_IMPORT_DIRECTORY;
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
    return X64_PE64_IMAGE_LOAD_STATUS_INVALID_ARGUMENT;
  }

  out_image_info->entry_point = 0;
  out_image_info->image_size = 0;
  out_image_info->import_count = 0;
  pe_image_load_result load_result {};
  const pe_image_load_config load_config {
    .expected_machine = PE_MACHINE_X64,
    .expected_image_base = expected_image_base,
    .expected_section_alignment = X64_USER_IMAGE_PAGE_SIZE,
    .expected_file_alignment = X64_USER_IMAGE_PAGE_SIZE,
    .loaded_image_size = loaded_image_size,
    .allow_imports = true,
    .allow_relocations = false,
  };
  const x64_pe64_image_load_status base_load_status
    = map_common_load_status(load_pe32_plus_image(image_bytes, image_size, loaded_image, load_config, &load_result));

  if (base_load_status != X64_PE64_IMAGE_LOAD_STATUS_OK)
  {
    return base_load_status;
  }

  const uint32_t loaded_image_size32 = static_cast<uint32_t>(loaded_image_size);

  if (load_result.import_directory.virtual_address != 0)
  {
    if (
      load_result.import_directory.virtual_address > load_result.image_size
      || load_result.import_directory.size > load_result.image_size - load_result.import_directory.virtual_address)
    {
      return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
    }

    size_t next_stub_offset = load_result.image_size;

    bool saw_terminator = false;
    const uint32_t descriptor_limit = load_result.import_directory.virtual_address + load_result.import_directory.size;

    for (uint32_t descriptor_rva = load_result.import_directory.virtual_address;
         descriptor_rva + sizeof(pe_import_descriptor) <= descriptor_limit;
         descriptor_rva += sizeof(pe_import_descriptor))
    {
      pe_import_descriptor descriptor {};

      if (!copy_loaded_record(&descriptor, sizeof(descriptor), loaded_image, load_result.image_size, descriptor_rva))
      {
        return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
      }

      if (is_empty_import_descriptor(descriptor))
      {
        saw_terminator = true;
        break;
      }

      uint32_t dll_name_rva = 0;
      uint32_t first_thunk_rva = 0;
      uint32_t lookup_table_rva = 0;

      if (
        !try_resolve_import_table_rva(descriptor.name, expected_image_base, load_result.image_size, &dll_name_rva)
        || !try_resolve_import_table_rva(
          descriptor.first_thunk, expected_image_base, load_result.image_size, &first_thunk_rva))
      {
        return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
      }

      if (descriptor.original_first_thunk != 0)
      {
        if (!try_resolve_import_table_rva(
              descriptor.original_first_thunk, expected_image_base, load_result.image_size, &lookup_table_rva))
        {
          return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
        }
      }
      else
      {
        lookup_table_rva = first_thunk_rva;
      }

      char dll_name[64] {};

      if (!read_loaded_ascii_string(loaded_image, load_result.image_size, dll_name_rva, dll_name, sizeof(dll_name)))
      {
        return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_NAME_OUT_OF_RANGE;
      }

      for (uint32_t thunk_index = 0;; ++thunk_index)
      {
        const uint32_t lookup_entry_rva = lookup_table_rva + (thunk_index * sizeof(uint64_t));
        const uint32_t address_entry_rva = first_thunk_rva + (thunk_index * sizeof(uint64_t));
        uint64_t lookup_entry = 0;

        if (!copy_loaded_record(
              &lookup_entry, sizeof(lookup_entry), loaded_image, load_result.image_size, lookup_entry_rva))
        {
          return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
        }

        uint8_t* address_entry
          = translate_loaded_rva(loaded_image, load_result.image_size, address_entry_rva, sizeof(uint64_t));

        if (address_entry == nullptr)
        {
          return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
        }

        if (lookup_entry == 0)
        {
          break;
        }

        if ((lookup_entry & PE_IMPORT_BY_ORDINAL_MASK64) != 0)
        {
          return X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_IMPORT_ORDINAL;
        }

        const uint64_t raw_name_reference = lookup_entry & ~PE_IMPORT_BY_ORDINAL_MASK64;
        uint32_t import_name_rva = 0;

        if (!try_resolve_import_name_rva(
              raw_name_reference, expected_image_base, load_result.image_size, &import_name_rva))
        {
          return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE;
        }

        char function_name[64] {};

        if (!read_loaded_ascii_string(
              loaded_image,
              load_result.image_size,
              import_name_rva + sizeof(uint16_t),
              function_name,
              sizeof(function_name)))
        {
          return X64_PE64_IMAGE_LOAD_STATUS_IMPORT_NAME_OUT_OF_RANGE;
        }

        uint64_t resolved_address = 0;
        const x64_pe64_image_load_status resolution_status = resolve_x64_import(
          import_resolver,
          dll_name,
          function_name,
          expected_image_base,
          loaded_image,
          loaded_image_size32,
          &next_stub_offset,
          &resolved_address);

        if (resolution_status != X64_PE64_IMAGE_LOAD_STATUS_OK)
        {
          return resolution_status;
        }

        memcpy(address_entry, &resolved_address, sizeof(resolved_address));
        ++out_image_info->import_count;
      }
    }

    if (!saw_terminator)
    {
      return X64_PE64_IMAGE_LOAD_STATUS_INVALID_IMPORT_DIRECTORY;
    }
  }

  out_image_info->entry_point = load_result.entry_point;
  out_image_info->image_size = load_result.image_size;
  return X64_PE64_IMAGE_LOAD_STATUS_OK;
}

const char* describe_x64_pe64_image_load_status(x64_pe64_image_load_status status)
{
  switch (status)
  {
  case X64_PE64_IMAGE_LOAD_STATUS_OK:
    return "x64 PE64 image loaded successfully";
  case X64_PE64_IMAGE_LOAD_STATUS_INVALID_ARGUMENT:
    return "x64 PE64 image loader received an invalid argument";
  case X64_PE64_IMAGE_LOAD_STATUS_MISSING_DOS_HEADER:
    return "x64 PE64 image is missing the DOS header";
  case X64_PE64_IMAGE_LOAD_STATUS_INVALID_DOS_HEADER:
    return "x64 PE64 image has an invalid DOS header";
  case X64_PE64_IMAGE_LOAD_STATUS_MISSING_NT_HEADERS:
    return "x64 PE64 image is missing the NT headers";
  case X64_PE64_IMAGE_LOAD_STATUS_INVALID_NT_SIGNATURE:
    return "x64 PE64 image has an invalid NT signature";
  case X64_PE64_IMAGE_LOAD_STATUS_WRONG_MACHINE:
    return "x64 PE64 image targets the wrong machine";
  case X64_PE64_IMAGE_LOAD_STATUS_MISSING_SECTIONS:
    return "x64 PE64 image does not define any sections";
  case X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_OPTIONAL_HEADER:
    return "x64 PE64 image has an unsupported optional header size";
  case X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_MAGIC:
    return "x64 PE64 image is not PE32+";
  case X64_PE64_IMAGE_LOAD_STATUS_UNEXPECTED_IMAGE_BASE:
    return "x64 PE64 image uses an unexpected image base";
  case X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_ALIGNMENT:
    return "x64 PE64 image must use 4 KiB section alignment";
  case X64_PE64_IMAGE_LOAD_STATUS_IMAGE_TOO_LARGE:
    return "x64 PE64 image does not fit in the initial user region";
  case X64_PE64_IMAGE_LOAD_STATUS_HEADERS_OUT_OF_RANGE:
    return "x64 PE64 image headers are out of range";
  case X64_PE64_IMAGE_LOAD_STATUS_ENTRY_POINT_OUT_OF_RANGE:
    return "x64 PE64 image entry point is out of range";
  case X64_PE64_IMAGE_LOAD_STATUS_INVALID_IMPORT_DIRECTORY:
    return "x64 PE64 image has an invalid import directory";
  case X64_PE64_IMAGE_LOAD_STATUS_UNEXPECTED_IMPORTS:
    return "x64 PE64 image unexpectedly imports system libraries";
  case X64_PE64_IMAGE_LOAD_STATUS_UNEXPECTED_RELOCATIONS:
    return "x64 PE64 image unexpectedly requires relocations";
  case X64_PE64_IMAGE_LOAD_STATUS_TRUNCATED_SECTION_TABLE:
    return "x64 PE64 image section table is truncated";
  case X64_PE64_IMAGE_LOAD_STATUS_SECTION_OUT_OF_RANGE:
    return "x64 PE64 image section exceeds the declared image size";
  case X64_PE64_IMAGE_LOAD_STATUS_SECTION_DATA_OUT_OF_RANGE:
    return "x64 PE64 image section data is out of range";
  case X64_PE64_IMAGE_LOAD_STATUS_IMPORT_TABLE_OUT_OF_RANGE:
    return "x64 PE64 image import table is out of range";
  case X64_PE64_IMAGE_LOAD_STATUS_IMPORT_NAME_OUT_OF_RANGE:
    return "x64 PE64 image import name is out of range";
  case X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_IMPORT:
    return "x64 PE64 image imports an unsupported Windows symbol";
  case X64_PE64_IMAGE_LOAD_STATUS_UNSUPPORTED_IMPORT_ORDINAL:
    return "x64 PE64 image imports by ordinal, which is not supported yet";
  case X64_PE64_IMAGE_LOAD_STATUS_IMPORT_STUB_OUT_OF_RANGE:
    return "x64 PE64 image import thunks do not fit in the initial user region";
  }

  return "x64 PE64 image loader failed with an unknown status";
}
