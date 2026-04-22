cmake_minimum_required(VERSION 3.27)

project(ringos_terminal_service LANGUAGES CXX ASM)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before configuring services/terminal.")
endif()

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure services/terminal with the published ringos toolchain bundle. "
    "Pass -DCMAKE_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake."
  )
endif()

add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/../../kernelsdk"
  "${CMAKE_CURRENT_BINARY_DIR}/ringos_kernelsdk")

get_filename_component(ringos_repo_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(terminal_service
  main.cpp)

target_include_directories(terminal_service
  PRIVATE
    "${ringos_repo_root}/kernel")

target_compile_options(terminal_service
  PRIVATE
    -fno-ms-compatibility
    -O2
    -Wall
    -Wextra
    -Wpedantic)

target_link_libraries(terminal_service
  PRIVATE
    ringos_kernelsdk)
