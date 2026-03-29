#pragma once

#include <stddef.h>
#include <stdint.h>

inline constexpr uint16_t PE_DOS_SIGNATURE = 0x5A4D;
inline constexpr uint32_t PE_NT_SIGNATURE = 0x00004550;
inline constexpr uint16_t PE_MACHINE_ARM64 = 0xAA64;
inline constexpr uint16_t PE_MACHINE_X64 = 0x8664;
inline constexpr uint16_t PE32_PLUS_MAGIC = 0x20B;
inline constexpr uint32_t PE_IMPORT_DIRECTORY_INDEX = 1;
inline constexpr uint32_t PE_BASE_RELOCATION_DIRECTORY_INDEX = 5;

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

enum class pe_image_load_status : uint32_t
{
  OK = 0,
  INVALID_ARGUMENT,
  MISSING_DOS_HEADER,
  INVALID_DOS_HEADER,
  MISSING_NT_HEADERS,
  INVALID_NT_SIGNATURE,
  WRONG_MACHINE,
  MISSING_SECTIONS,
  UNSUPPORTED_OPTIONAL_HEADER,
  UNSUPPORTED_MAGIC,
  UNEXPECTED_IMAGE_BASE,
  UNSUPPORTED_ALIGNMENT,
  IMAGE_TOO_LARGE,
  HEADERS_OUT_OF_RANGE,
  ENTRY_POINT_OUT_OF_RANGE,
  INVALID_IMPORT_DIRECTORY,
  UNEXPECTED_IMPORTS,
  UNEXPECTED_RELOCATIONS,
  TRUNCATED_SECTION_TABLE,
  SECTION_OUT_OF_RANGE,
  SECTION_DATA_OUT_OF_RANGE,
};

struct pe_image_load_config
{
  uint16_t expected_machine;
  uintptr_t expected_image_base;
  uint32_t expected_section_alignment;
  uint32_t expected_file_alignment;
  size_t loaded_image_size;
  bool allow_imports;
  bool allow_relocations;
};

struct pe_image_load_result
{
  uintptr_t entry_point;
  uint32_t image_size;
  pe_data_directory import_directory;
};

bool try_get_pe_machine(const uint8_t* image_bytes, size_t image_size, uint16_t* out_machine);
pe_image_load_status load_pe32_plus_image(
  const uint8_t* image_bytes,
  size_t image_size,
  uint8_t* loaded_image,
  const pe_image_load_config& config,
  pe_image_load_result* out_result);
const char* describe_pe_image_load_status(pe_image_load_status status);
