cmake_minimum_required(VERSION 3.27)

project(ringos_c_runtime LANGUAGES C CXX)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before configuring user/libc.")
endif()

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure user/libc with the published ringos toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake."
  )
endif()

set(libc_include_dir "${CMAKE_CURRENT_LIST_DIR}/include")
set(sdk_include_dir "${CMAKE_CURRENT_LIST_DIR}/../sdk/include")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ringos_c STATIC
  "${CMAKE_CURRENT_LIST_DIR}/src/errno.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/puts.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/stdio.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/exit.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/stdlib.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/src/string.cpp")

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