cmake_minimum_required(VERSION 3.27)

project(ringos_kernelsdk_runtime LANGUAGES C CXX ASM)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before configuring kernelsdk.")
endif()

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure kernelsdk with the published ringos toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake.")
endif()

if(RINGOS_TARGET_ARCH STREQUAL "x64")
  set(syscall_source "${CMAKE_CURRENT_LIST_DIR}/x64/ringos_syscall.S")
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  set(syscall_source "${CMAKE_CURRENT_LIST_DIR}/arm64/ringos_syscall.S")
else()
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: ${RINGOS_TARGET_ARCH}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ringos_kernelsdk STATIC
  "${syscall_source}"
  "${CMAKE_CURRENT_LIST_DIR}/src/ringos_debug.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/ringos_process.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/ringos_rpc.cpp")

target_include_directories(ringos_kernelsdk
  PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")

target_compile_options(ringos_kernelsdk
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

set_target_properties(ringos_kernelsdk PROPERTIES
  OUTPUT_NAME "ringos_kernelsdk"
  PREFIX ""
  SUFFIX ".lib")