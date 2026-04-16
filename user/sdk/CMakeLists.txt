cmake_minimum_required(VERSION 3.27)

project(ringos_sdk_runtime LANGUAGES C CXX ASM)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before configuring user/sdk.")
endif()

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure user/sdk with the published ringos toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake."
  )
endif()

set(sdk_stage_root "${RINGOS_SDK_STAGE_ROOT}")
set(sdk_include_dir "${CMAKE_CURRENT_LIST_DIR}/include")
set(libc_include_dir "${CMAKE_CURRENT_LIST_DIR}/../libc/include")

if(NOT sdk_stage_root STREQUAL "")
  set(sdk_stage_include_dir "${sdk_stage_root}/include")
  set(sdk_stage_lib_dir "${sdk_stage_root}/lib")
  file(MAKE_DIRECTORY "${sdk_stage_root}" "${sdk_stage_lib_dir}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(common_c_flags
  -O2
  -ffreestanding
  -fno-stack-protector
  -fno-builtin
  -Wall
  -Wextra
  -Wpedantic)

set(common_cxx_flags
  -O2
  -ffreestanding
  -fno-exceptions
  -fno-rtti
  -fno-stack-protector
  -fno-builtin
  -nostdinc++
  -Wall
  -Wextra
  -Wpedantic)

if(RINGOS_TARGET_ARCH STREQUAL "x64")
  set(syscall_source "${CMAKE_CURRENT_LIST_DIR}/x64/ringos_syscall.S")
  set(runtime_extra_flags)
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  set(syscall_source "${CMAKE_CURRENT_LIST_DIR}/arm64/ringos_syscall.S")
  set(runtime_extra_flags)
else()
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: ${RINGOS_TARGET_ARCH}")
endif()

add_library(ringos_sdk STATIC
  "${syscall_source}"
  "${CMAKE_CURRENT_LIST_DIR}/src/ringos_debug.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/ringos_process.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/ringos_rpc.cpp")

target_include_directories(ringos_sdk
  PUBLIC
    "${sdk_include_dir}")

target_compile_options(ringos_sdk
  PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-O2>"
    "$<$<COMPILE_LANGUAGE:CXX>:-ffreestanding>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-builtin>"
    "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>")

set_target_properties(ringos_sdk PROPERTIES
  OUTPUT_NAME "ringos_sdk"
  PREFIX ""
  SUFFIX ".lib")

if(NOT sdk_stage_root STREQUAL "")
  set_target_properties(ringos_sdk PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${sdk_stage_lib_dir}")
endif()

add_library(ringos_c STATIC
  "${CMAKE_CURRENT_LIST_DIR}/../libc/src/errno.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../libc/src/puts.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../libc/src/stdio.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../libc/src/exit.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../libc/src/stdlib.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../libc/src/string.cpp")

target_include_directories(ringos_c
  PUBLIC
    "${libc_include_dir}"
  PRIVATE
    "${sdk_include_dir}")

target_compile_options(ringos_c
  PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-O2>"
    "$<$<COMPILE_LANGUAGE:C>:-ffreestanding>"
    "$<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>"
    "$<$<COMPILE_LANGUAGE:C>:-fno-builtin>"
    "$<$<COMPILE_LANGUAGE:C>:-Wall>"
    "$<$<COMPILE_LANGUAGE:C>:-Wextra>"
    "$<$<COMPILE_LANGUAGE:C>:-Wpedantic>"
    "$<$<COMPILE_LANGUAGE:CXX>:-O2>"
    "$<$<COMPILE_LANGUAGE:CXX>:-ffreestanding>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>"
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-builtin>"
    "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>")

set_target_properties(ringos_c PROPERTIES
  OUTPUT_NAME "ringos_c"
  PREFIX ""
  SUFFIX ".lib"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_library(ringos_crt0 OBJECT
  "${CMAKE_CURRENT_LIST_DIR}/../crt/src/crt0.c")

target_include_directories(ringos_crt0
  PRIVATE
    "${libc_include_dir}")

target_compile_options(ringos_crt0
  PRIVATE
    ${common_c_flags}
    ${runtime_extra_flags})

if(NOT sdk_stage_root STREQUAL "")
  add_custom_target(ringos_sdk_stage
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${sdk_stage_include_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${sdk_stage_include_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${sdk_include_dir}" "${sdk_stage_include_dir}"
    DEPENDS
      ringos_sdk
      "${sdk_include_dir}/ringos/debug.h"
      "${sdk_include_dir}/ringos/handle.h"
      "${sdk_include_dir}/ringos/process.h"
      "${sdk_include_dir}/ringos/rpc.h"
      "${sdk_include_dir}/ringos/sdk.h"
      "${sdk_include_dir}/ringos/status.h"
      "${sdk_include_dir}/ringos/syscalls.h"
      "${sdk_include_dir}/ringos/types.h"
    COMMENT "Staging RingOS SDK headers under ${sdk_stage_root}"
    VERBATIM)
endif()

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/crt0.obj"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:ringos_crt0> "${CMAKE_CURRENT_BINARY_DIR}/crt0.obj"
  DEPENDS ringos_crt0
  COMMAND_EXPAND_LISTS
  COMMENT "Writing RingOS crt0 object into ${CMAKE_CURRENT_BINARY_DIR}"
  VERBATIM)

add_custom_target(ringos_toolchain_runtime
  DEPENDS
    ringos_c
    "${CMAKE_CURRENT_BINARY_DIR}/crt0.obj")
