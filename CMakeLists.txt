cmake_minimum_required(VERSION 3.27)

project(ringos LANGUAGES C CXX ASM)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR "RINGOS_TARGET_ARCH is not set. Use a CMake preset (e.g. cmake --preset x64-debug).")
endif()

if(NOT RINGOS_TARGET_ARCH STREQUAL "x64" AND NOT RINGOS_TARGET_ARCH STREQUAL "arm64")
  message(FATAL_ERROR "RINGOS_TARGET_ARCH must be 'x64' or 'arm64'. Got: ${RINGOS_TARGET_ARCH}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RINGOS_ENABLE_TESTING "Enable smoke tests" OFF)
option(RINGOS_CI "Enable CI mode (disables non-essential developer options)" OFF)

add_subdirectory(kernel)
add_subdirectory(arch/${RINGOS_TARGET_ARCH})

if(RINGOS_ENABLE_TESTING)
  include(CTest)
  include(cmake/tests/smoke_tests.cmake)
endif()
