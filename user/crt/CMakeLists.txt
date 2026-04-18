cmake_minimum_required(VERSION 3.27)

project(ringos_crt_runtime LANGUAGES C)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before configuring user/crt.")
endif()

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure user/crt with the published ringos toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake."
  )
endif()

if(NOT RINGOS_TARGET_ARCH STREQUAL "x64" AND NOT RINGOS_TARGET_ARCH STREQUAL "arm64")
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: ${RINGOS_TARGET_ARCH}")
endif()

set(libc_include_dir "${CMAKE_CURRENT_LIST_DIR}/../libc/include")
set(crt0_output_path "${CMAKE_CURRENT_BINARY_DIR}/crt0.obj")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(common_c_flags
  -O2
  -ffreestanding
  -fno-stack-protector
  -fno-builtin
  -Wall
  -Wextra
  -Wpedantic)

add_library(ringos_crt0 OBJECT
  "${CMAKE_CURRENT_LIST_DIR}/src/crt0.c")

target_include_directories(ringos_crt0
  PRIVATE
    "${libc_include_dir}")

target_compile_options(ringos_crt0
  PRIVATE
    ${common_c_flags})

add_custom_command(
  OUTPUT "${crt0_output_path}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:ringos_crt0> "${crt0_output_path}"
  DEPENDS ringos_crt0
  COMMAND_EXPAND_LISTS
  COMMENT "Writing RingOS crt0 object into ${crt0_output_path}"
  VERBATIM)

add_custom_target(ringos_crt0_object
  DEPENDS
    "${crt0_output_path}")