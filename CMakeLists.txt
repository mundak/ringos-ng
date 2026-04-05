cmake_minimum_required(VERSION 3.27)

project(ringos LANGUAGES C CXX ASM)

include(cmake/ringos_toolchain_common.cmake)
include(cmake/ringos_installed_toolchain.cmake)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR "RINGOS_TARGET_ARCH is not set. Use a CMake preset (e.g. cmake --preset x64-debug).")
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
set(RINGOS_TOOLCHAIN_DRIVER_MODE "ringos-native"
  CACHE STRING "Compiler driver mode for the shared ringos toolchain package")
set_property(CACHE RINGOS_TOOLCHAIN_DRIVER_MODE PROPERTY STRINGS ringos-native)

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
