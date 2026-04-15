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

if(NOT DEFINED RINGOS_SDK_STAGE_ROOT OR RINGOS_SDK_STAGE_ROOT STREQUAL "")
  message(FATAL_ERROR "Set RINGOS_SDK_STAGE_ROOT to the sysroot staging directory.")
endif()

if(NOT DEFINED RINGOS_LIBCXX_INCLUDE_DIR OR RINGOS_LIBCXX_INCLUDE_DIR STREQUAL "")
  message(FATAL_ERROR "Set RINGOS_LIBCXX_INCLUDE_DIR to the libc++ include directory.")
endif()

if(NOT EXISTS "${RINGOS_LIBCXX_INCLUDE_DIR}/__config")
  message(FATAL_ERROR "RINGOS_LIBCXX_INCLUDE_DIR does not contain libc++ headers: ${RINGOS_LIBCXX_INCLUDE_DIR}")
endif()

set(stage_root "${RINGOS_SDK_STAGE_ROOT}")
set(stage_include_dir "${stage_root}/include")
set(stage_cxx_include_dir "${stage_include_dir}/c++/v1")
set(stage_lib_dir "${stage_root}/lib")
set(sdk_include_dir "${CMAKE_CURRENT_LIST_DIR}/include")
set(libc_include_dir "${CMAKE_CURRENT_LIST_DIR}/../libc/include")
set(libcxx_overlay_dir "${CMAKE_CURRENT_LIST_DIR}/../libcxx")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(MAKE_DIRECTORY "${stage_root}" "${stage_lib_dir}")

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
  SUFFIX ".lib"
  ARCHIVE_OUTPUT_DIRECTORY "${stage_lib_dir}")

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
  ARCHIVE_OUTPUT_DIRECTORY "${stage_lib_dir}")

add_library(clang_rt_builtins STATIC
  "${CMAKE_CURRENT_LIST_DIR}/../compiler_rt/src/builtins.c")

target_compile_options(clang_rt_builtins
  PRIVATE
    ${common_c_flags})

set_target_properties(clang_rt_builtins PROPERTIES
  OUTPUT_NAME "clang_rt.builtins"
  PREFIX ""
  SUFFIX ".lib"
  ARCHIVE_OUTPUT_DIRECTORY "${stage_lib_dir}")

add_library(ringos_crt0 OBJECT
  "${CMAKE_CURRENT_LIST_DIR}/../crt/src/crt0.c")

target_include_directories(ringos_crt0
  PRIVATE
    "${libc_include_dir}")

target_compile_options(ringos_crt0
  PRIVATE
    ${common_c_flags}
    ${runtime_extra_flags})

if(DEFINED RINGOS_LIBCXX_OVERLAY_DIR AND NOT RINGOS_LIBCXX_OVERLAY_DIR STREQUAL "")
  set(libcxx_overlay_dir "${RINGOS_LIBCXX_OVERLAY_DIR}")
endif()

if(EXISTS "${libcxx_overlay_dir}")
  set(libcxx_overlay_copy_command
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${libcxx_overlay_dir}" "${stage_cxx_include_dir}")
else()
  set(libcxx_overlay_copy_command)
endif()

add_custom_command(
  OUTPUT "${stage_root}/.headers.stamp"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${stage_include_dir}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${stage_include_dir}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${stage_include_dir}/c++"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${sdk_include_dir}" "${stage_include_dir}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${libc_include_dir}" "${stage_include_dir}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${RINGOS_LIBCXX_INCLUDE_DIR}" "${stage_cxx_include_dir}"
  ${libcxx_overlay_copy_command}
  COMMAND "${CMAKE_COMMAND}" -E touch "${stage_root}/.headers.stamp"
  DEPENDS
    "${sdk_include_dir}/ringos/debug.h"
    "${sdk_include_dir}/ringos/handle.h"
    "${sdk_include_dir}/ringos/process.h"
    "${sdk_include_dir}/ringos/rpc.h"
    "${sdk_include_dir}/ringos/sdk.h"
    "${sdk_include_dir}/ringos/status.h"
    "${sdk_include_dir}/ringos/syscalls.h"
    "${sdk_include_dir}/ringos/types.h"
    "${libc_include_dir}/errno.h"
    "${libc_include_dir}/stdio.h"
    "${libc_include_dir}/stdlib.h"
    "${libc_include_dir}/string.h"
  COMMENT "Staging RingOS SDK headers under ${stage_root}"
  VERBATIM)

add_custom_command(
  OUTPUT "${stage_lib_dir}/crt0.obj"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${stage_lib_dir}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_OBJECTS:ringos_crt0> "${stage_lib_dir}/crt0.obj"
  DEPENDS ringos_crt0
  COMMAND_EXPAND_LISTS
  COMMENT "Staging RingOS crt0 object under ${stage_root}"
  VERBATIM)

add_custom_target(ringos_sdk_stage
  DEPENDS
    ringos_sdk
    ringos_c
    clang_rt_builtins
    "${stage_root}/.headers.stamp"
    "${stage_lib_dir}/crt0.obj")
