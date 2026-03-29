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
  IMPORT_TABLE_OUT_OF_RANGE,
  IMPORT_NAME_OUT_OF_RANGE,
  UNSUPPORTED_IMPORT,
  UNSUPPORTED_IMPORT_ORDINAL,
  IMPORT_STUB_OUT_OF_RANGE,
};

struct x64_pe64_image_info
{
  uintptr_t entry_point;
  uint32_t image_size;
  uint32_t import_count;
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

