cmake_minimum_required(VERSION 3.27)

project(ringos LANGUAGES C CXX ASM)

include(tools/toolchain/ringos_toolchain_common.cmake)
include(tools/toolchain/ringos_installed_toolchain.cmake)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "RINGOS_TARGET_ARCH is not set. Configure with -DRINGOS_TARGET_ARCH=<x64|arm64> "
    "and the matching kernel toolchain file (for example, "
    "cmake -S . -B build/x64-debug -G Ninja "
    "-DCMAKE_TOOLCHAIN_FILE=kernel/toolchains/x64.cmake -DRINGOS_TARGET_ARCH=x64).")
endif()

if(NOT RINGOS_TARGET_ARCH STREQUAL "x64" AND NOT RINGOS_TARGET_ARCH STREQUAL "arm64")
  message(FATAL_ERROR "RINGOS_TARGET_ARCH must be 'x64' or 'arm64'. Got: ${RINGOS_TARGET_ARCH}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

ringos_get_default_toolchain_install_root(ringos_default_toolchain_install_root)
set(RINGOS_TOOLCHAIN_ROOT ${ringos_default_toolchain_install_root}
  CACHE PATH "Root of the shared ringos toolchain package")

option(RINGOS_ENABLE_TESTING "Enable smoke tests" OFF)
add_subdirectory(win32)
add_subdirectory(emulator)
add_subdirectory(kernel)
add_subdirectory(arch/${RINGOS_TARGET_ARCH})

if(TARGET ringos_${RINGOS_TARGET_ARCH}_sdk_sysroot)
  add_custom_target(ringos_sdk_sysroot DEPENDS ringos_${RINGOS_TARGET_ARCH}_sdk_sysroot)
endif()

if(TARGET ringos_${RINGOS_TARGET_ARCH}_installed_toolchain)
  add_custom_target(ringos_installed_toolchain DEPENDS ringos_${RINGOS_TARGET_ARCH}_installed_toolchain)
endif()

if(TARGET ringos_x64_sdk_sysroot AND TARGET ringos_arm64_sdk_sysroot)
  add_custom_target(ringos_all_sdk_sysroots DEPENDS ringos_x64_sdk_sysroot ringos_arm64_sdk_sysroot)
endif()

if(TARGET ringos_x64_installed_toolchain AND TARGET ringos_arm64_installed_toolchain)
  add_custom_target(ringos_all_installed_toolchains DEPENDS ringos_x64_installed_toolchain ringos_arm64_installed_toolchain)
endif()

if(RINGOS_ENABLE_TESTING)
  include(CTest)
  include(cmake/tests/smoke_tests.cmake)
  include(cmake/tests/emulator_tests.cmake)
  add_subdirectory(win32/tests)
endif()
