cmake_minimum_required(VERSION 3.27)

project(ringos_console_service_write LANGUAGES CXX)

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure this sample with the published ringos toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake "
    "and -DRINGOS_TARGET_ARCH=<x64|arm64>."
  )
endif()

add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/../../../kernelsdk"
  "${CMAKE_CURRENT_BINARY_DIR}/ringos_kernelsdk")

get_filename_component(ringos_repo_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(console_service_write
  console_service_write.cpp
)

target_include_directories(console_service_write
  PRIVATE
    "${ringos_repo_root}/kernel"
)

target_compile_options(console_service_write
  PRIVATE
    -fno-ms-compatibility
    -O2
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(console_service_write
  PRIVATE
    ringos_kernelsdk
)
