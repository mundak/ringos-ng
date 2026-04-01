#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

enum x64_win32_import_resolution_status : uint32_t
{
  X64_WIN32_IMPORT_RESOLUTION_STATUS_OK = 0,
  X64_WIN32_IMPORT_RESOLUTION_STATUS_INVALID_ARGUMENT,
  X64_WIN32_IMPORT_RESOLUTION_STATUS_DLL_NOT_FOUND,
  X64_WIN32_IMPORT_RESOLUTION_STATUS_SYMBOL_NOT_FOUND,
  X64_WIN32_IMPORT_RESOLUTION_STATUS_UNSUPPORTED_SYSCALL_NUMBER,
  X64_WIN32_IMPORT_RESOLUTION_STATUS_STUB_OUT_OF_SPACE,
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
