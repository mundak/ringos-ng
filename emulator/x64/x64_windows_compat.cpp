#include "x64_windows_compat.h"

namespace
{
  struct x64_windows_import_mapping
  {
    const char* dll_name;
    const char* function_name;
    uint32_t syscall_number;
  };

  constexpr x64_windows_import_mapping X64_WINDOWS_IMPORT_MAPPINGS[] {
    { "kernel32", "GetStdHandle", static_cast<uint32_t>(X64_WINDOWS_SYSCALL_GET_STD_HANDLE) },
    { "kernel32", "WriteFile", static_cast<uint32_t>(X64_WINDOWS_SYSCALL_WRITE_FILE) },
    { "kernel32", "ExitProcess", static_cast<uint32_t>(X64_WINDOWS_SYSCALL_EXIT_PROCESS) },
  };

  char to_ascii_lower(char value)
  {
    if (value >= 'A' && value <= 'Z')
    {
      return static_cast<char>(value - 'A' + 'a');
    }

    return value;
  }

  bool equals_ascii_ignore_case(const char* lhs, const char* rhs)
  {
    if (lhs == nullptr || rhs == nullptr)
    {
      return false;
    }

    while (*lhs != '\0' && *rhs != '\0')
    {
      if (to_ascii_lower(*lhs) != to_ascii_lower(*rhs))
      {
        return false;
      }

      ++lhs;
      ++rhs;
    }

    return *lhs == '\0' && *rhs == '\0';
  }

  bool matches_windows_module_name(const char* candidate, const char* expected_base_name)
  {
    if (candidate == nullptr || expected_base_name == nullptr)
    {
      return false;
    }

    if (equals_ascii_ignore_case(candidate, expected_base_name))
    {
      return true;
    }

    constexpr char dll_suffix[] = ".dll";
    char module_name_with_suffix[32] {};
    uint32_t write_index = 0;

    while (expected_base_name[write_index] != '\0' && write_index + 1 < sizeof(module_name_with_suffix))
    {
      module_name_with_suffix[write_index] = expected_base_name[write_index];
      ++write_index;
    }

    for (uint32_t suffix_index = 0;
         dll_suffix[suffix_index] != '\0' && write_index + 1 < sizeof(module_name_with_suffix);
         ++suffix_index)
    {
      module_name_with_suffix[write_index] = dll_suffix[suffix_index];
      ++write_index;
    }

    module_name_with_suffix[write_index] = '\0';
    return equals_ascii_ignore_case(candidate, module_name_with_suffix);
  }
}

bool try_resolve_x64_windows_import(const char* dll_name, const char* function_name, uint32_t* out_syscall_number)
{
  if (dll_name == nullptr || function_name == nullptr || out_syscall_number == nullptr)
  {
    return false;
  }

  for (const x64_windows_import_mapping& mapping : X64_WINDOWS_IMPORT_MAPPINGS)
  {
    if (!matches_windows_module_name(dll_name, mapping.dll_name))
    {
      continue;
    }

    if (!equals_ascii_ignore_case(function_name, mapping.function_name))
    {
      continue;
    }

    *out_syscall_number = mapping.syscall_number;
    return true;
  }

  return false;
}

