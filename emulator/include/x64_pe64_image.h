#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

inline constexpr size_t X64_USER_IMAGE_PAGE_SIZE = 4096;
inline constexpr uintptr_t X64_USER_IMAGE_VIRTUAL_ADDRESS = 0x400000;
inline constexpr size_t X64_USER_IMAGE_PAGE_COUNT = 8;
inline constexpr uintptr_t X64_USER_STACK_VIRTUAL_ADDRESS
  = X64_USER_IMAGE_VIRTUAL_ADDRESS + (X64_USER_IMAGE_PAGE_COUNT * X64_USER_IMAGE_PAGE_SIZE);
inline constexpr size_t X64_USER_REGION_SIZE = (X64_USER_IMAGE_PAGE_COUNT + 1) * X64_USER_IMAGE_PAGE_SIZE;

struct x64_pe64_import_resolver
{
  void* context;
  bool (*resolve_import)(void* context, const char* dll_name, const char* function_name, uint32_t* out_syscall_number);
};

enum class x64_pe64_image_load_status : uint32_t
{
  ok = 0,
  invalid_argument,
  missing_dos_header,
  invalid_dos_header,
  missing_nt_headers,
  invalid_nt_signature,
  wrong_machine,
  missing_sections,
  unsupported_optional_header,
  unsupported_magic,
  unexpected_image_base,
  unsupported_alignment,
  image_too_large,
  headers_out_of_range,
  entry_point_out_of_range,
  invalid_import_directory,
  unexpected_imports,
  unexpected_relocations,
  truncated_section_table,
  section_out_of_range,
  section_data_out_of_range,
  import_table_out_of_range,
  import_name_out_of_range,
  unsupported_import,
  unsupported_import_ordinal,
  import_stub_out_of_range,
};

struct x64_pe64_image_info
{
  uintptr_t entry_point;
  uint32_t image_size;
};

x64_pe64_image_load_status load_x64_pe64_image(
  const uint8_t* image_bytes,
  size_t image_size,
  uintptr_t expected_image_base,
  uint8_t* loaded_image,
  size_t loaded_image_size,
  const x64_pe64_import_resolver* import_resolver,
  x64_pe64_image_info* out_image_info);
const char* describe_x64_pe64_image_load_status(x64_pe64_image_load_status status);
