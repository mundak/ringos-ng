#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

enum class x64_win32_import_resolution_status : uint32_t
{
  ok = 0,
  invalid_argument,
  dll_not_found,
  symbol_not_found,
  unsupported_syscall_number,
  stub_out_of_space,
};

x64_win32_import_resolution_status resolve_x64_win32_import(
  const char* dll_name,
  const char* symbol_name,
  uintptr_t image_base,
  uint8_t* loaded_image,
  size_t loaded_image_size,
  size_t* inout_next_stub_offset,
  uint64_t* out_function_address);
const char* describe_x64_win32_import_resolution_status(x64_win32_import_resolution_status status);

