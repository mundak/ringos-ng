#pragma once

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstdint>
#else
#include <stdint.h>
#endif

inline constexpr uint64_t X64_WINDOWS_SYSCALL_GET_STD_HANDLE = 0x100;
inline constexpr uint64_t X64_WINDOWS_SYSCALL_WRITE_FILE = 0x101;
inline constexpr uint64_t X64_WINDOWS_SYSCALL_EXIT_PROCESS = 0x102;

inline constexpr int32_t X64_WINDOWS_STD_INPUT_HANDLE = -10;
inline constexpr int32_t X64_WINDOWS_STD_OUTPUT_HANDLE = -11;
inline constexpr int32_t X64_WINDOWS_STD_ERROR_HANDLE = -12;

bool try_resolve_x64_windows_import(const char* dll_name, const char* function_name, uint32_t* out_syscall_number);
