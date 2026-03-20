cmake_minimum_required(VERSION 3.27)

project(ringos
  LANGUAGES C CXX ASM
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR "RINGOS_TARGET_ARCH must be set. Supported values: x64, arm64")
endif()

add_subdirectory(kernel)

if(RINGOS_TARGET_ARCH STREQUAL "x64")
  add_subdirectory(arch/x64)
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  add_subdirectory(arch/arm64)
else()
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: ${RINGOS_TARGET_ARCH}. Supported values: x64, arm64")
endif()

if(RINGOS_ENABLE_TESTING)
  include(CTest)
  include(cmake/tests/smoke_tests.cmake)
endif()
