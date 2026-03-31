#include "x64_win32_emulation.h"

#include "x64_win32_dll_exports.h"

namespace
{
  constexpr size_t X64_WIN32_IMPORT_STUB_SIZE = 11;

  size_t align_up(size_t value, size_t alignment)
  {
    if (alignment == 0)
    {
      return value;
    }

    const size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
  }

  char to_lower_ascii(char value)
  {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
  }

  bool has_dll_suffix(const char* value, size_t start_index)
  {
    return value[start_index] != '\0' && to_lower_ascii(value[start_index]) == 'd'
      && to_lower_ascii(value[start_index + 1]) == 'l' && to_lower_ascii(value[start_index + 2]) == 'l'
      && value[start_index + 3] == '\0';
  }

  bool dll_names_match(const char* imported_name, const char* registered_name)
  {
    if (imported_name == nullptr || registered_name == nullptr)
    {
      return false;
    }

    size_t index = 0;

    while (registered_name[index] != '\0')
    {
      if (
        imported_name[index] == '\0' || to_lower_ascii(imported_name[index]) != to_lower_ascii(registered_name[index]))
      {
        return false;
      }

      ++index;
    }

    if (imported_name[index] == '\0')
    {
      return true;
    }

    return imported_name[index] == '.' && has_dll_suffix(imported_name, index + 1);
  }

  bool symbol_names_match(const char* left, const char* right)
  {
    if (left == nullptr || right == nullptr)
    {
      return false;
    }

    size_t index = 0;

    while (left[index] != '\0' && right[index] != '\0')
    {
      if (left[index] != right[index])
      {
        return false;
      }

      ++index;
    }

    return left[index] == '\0' && right[index] == '\0';
  }

  void write_u32_le(uint8_t* bytes, uint32_t value)
  {
    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
  }
}

x64_win32_import_resolution_status resolve_x64_win32_import(
  const char* dll_name,
  const char* symbol_name,
  uintptr_t image_base,
  uint8_t* loaded_image,
  size_t loaded_image_size,
  size_t* inout_next_stub_offset,
  uint64_t* out_function_address)
{
  if (
    dll_name == nullptr || symbol_name == nullptr || loaded_image == nullptr || inout_next_stub_offset == nullptr
    || out_function_address == nullptr)
  {
    return X64_WIN32_IMPORT_RESOLUTION_STATUS_INVALID_ARGUMENT;
  }

  size_t export_count = 0;
  const x64_win32_dll_export* exports = get_x64_win32_dll_exports(&export_count);
  bool saw_matching_dll = false;
  const x64_win32_dll_export* resolved_export = nullptr;

  for (size_t index = 0; index < export_count; ++index)
  {
    const x64_win32_dll_export& current_export = exports[index];

    if (!dll_names_match(dll_name, current_export.dll_name))
    {
      continue;
    }

    saw_matching_dll = true;

    if (symbol_names_match(symbol_name, current_export.symbol_name))
    {
      resolved_export = &current_export;
      break;
    }
  }

  if (resolved_export == nullptr)
  {
    return saw_matching_dll ? X64_WIN32_IMPORT_RESOLUTION_STATUS_SYMBOL_NOT_FOUND
                            : X64_WIN32_IMPORT_RESOLUTION_STATUS_DLL_NOT_FOUND;
  }

  if (resolved_export->syscall_number > 0xFFFFFFFFULL)
  {
    return X64_WIN32_IMPORT_RESOLUTION_STATUS_UNSUPPORTED_SYSCALL_NUMBER;
  }

  const size_t stub_offset = align_up(*inout_next_stub_offset, 16);

  if (stub_offset > loaded_image_size || X64_WIN32_IMPORT_STUB_SIZE > loaded_image_size - stub_offset)
  {
    return X64_WIN32_IMPORT_RESOLUTION_STATUS_STUB_OUT_OF_SPACE;
  }

  uint8_t* stub_bytes = loaded_image + stub_offset;
  stub_bytes[0] = 0x48;
  stub_bytes[1] = 0x89;
  stub_bytes[2] = 0xCF;
  stub_bytes[3] = 0xB8;
  write_u32_le(stub_bytes + 4, static_cast<uint32_t>(resolved_export->syscall_number));
  stub_bytes[8] = 0x0F;
  stub_bytes[9] = 0x05;
  stub_bytes[10] = 0xC3;

  *out_function_address = image_base + stub_offset;
  *inout_next_stub_offset = stub_offset + X64_WIN32_IMPORT_STUB_SIZE;
  return X64_WIN32_IMPORT_RESOLUTION_STATUS_OK;
}

const char* describe_x64_win32_import_resolution_status(x64_win32_import_resolution_status status)
{
  switch (status)
  {
  case X64_WIN32_IMPORT_RESOLUTION_STATUS_OK:
    return "x64 Win32 import resolved";
  case X64_WIN32_IMPORT_RESOLUTION_STATUS_INVALID_ARGUMENT:
    return "x64 Win32 import resolver received an invalid argument";
  case X64_WIN32_IMPORT_RESOLUTION_STATUS_DLL_NOT_FOUND:
    return "x64 Win32 import DLL is not registered";
  case X64_WIN32_IMPORT_RESOLUTION_STATUS_SYMBOL_NOT_FOUND:
    return "x64 Win32 import symbol is not registered";
  case X64_WIN32_IMPORT_RESOLUTION_STATUS_UNSUPPORTED_SYSCALL_NUMBER:
    return "x64 Win32 import maps to a syscall number that does not fit in the stub ABI";
  case X64_WIN32_IMPORT_RESOLUTION_STATUS_STUB_OUT_OF_SPACE:
    return "x64 Win32 import stub region is exhausted";
  }

  return "x64 Win32 import resolver failed with an unknown status";
}

