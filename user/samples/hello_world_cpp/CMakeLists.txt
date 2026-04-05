cmake_minimum_required(VERSION 3.27)

project(ringos_hello_world_cpp LANGUAGES CXX)

if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR
    "Configure this sample with the published ringos installed-toolchain bundle. "
    "Use -DCMAKE_TOOLCHAIN_FILE=<install-root>/cmake/ringos-toolchain.cmake "
    "and -DRINGOS_TARGET_ARCH=<x64|arm64>, or use the matching "
    "ringos-<arch>-toolchain.cmake file from the same install root. "
    "The default install root is %LOCALAPPDATA%/ringos/toolchain on Windows "
    "and ~/.cache/ringos/toolchain on Linux or macOS.")
endif()

if(NOT DEFINED RINGOS_SYSROOT_CXX_INCLUDE_DIR OR NOT EXISTS "${RINGOS_SYSROOT_CXX_INCLUDE_DIR}/type_traits")
  message(FATAL_ERROR
    "This toolchain bundle does not include staged libc++ headers. "
    "Refresh the installed toolchain bundle by rebuilding it with tools/toolchain/build-toolchain.sh.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hello_world_cpp
  hello_world.cpp
)

set_target_properties(hello_world_cpp
  PROPERTIES
    SUFFIX ".exe"
)

target_compile_options(hello_world_cpp
  PRIVATE
    -O2
    -Wall
    -Wextra
    -Wpedantic
)
