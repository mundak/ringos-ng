cmake_minimum_required(VERSION 3.27)

project(ringos_console_service_write LANGUAGES C)

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure this sample with the published ringos installed-toolchain bundle. "
    "Use -DCMAKE_TOOLCHAIN_FILE=<install-root>/cmake/ringos-toolchain.cmake "
    "and -DRINGOS_TARGET_ARCH=<x64|arm64>, or use the matching "
    "ringos-<arch>-toolchain.cmake file from the same install root. "
    "The default install root is %LOCALAPPDATA%/ringos/toolchain on Windows "
    "and ~/.cache/ringos/toolchain on Linux or macOS.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(console_service_write
  console_service_write.c
)

set_target_properties(console_service_write
  PROPERTIES
    SUFFIX ".exe"
)

target_compile_options(console_service_write
  PRIVATE
    -O2
    -Wall
    -Wextra
    -Wpedantic
)
