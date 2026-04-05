cmake_minimum_required(VERSION 3.27)

project(ringos_toolchain LANGUAGES NONE)

include(${CMAKE_CURRENT_LIST_DIR}/ringos_toolchain_common.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ringos_installed_toolchain.cmake)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR "RINGOS_TARGET_ARCH must be set to 'x64' or 'arm64'.")
endif()

if(NOT RINGOS_TARGET_ARCH STREQUAL "x64" AND NOT RINGOS_TARGET_ARCH STREQUAL "arm64")
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: ${RINGOS_TARGET_ARCH}")
endif()

ringos_get_default_toolchain_install_root(ringos_default_toolchain_install_root)
set(RINGOS_TOOLCHAIN_ROOT ${ringos_default_toolchain_install_root}
  CACHE PATH "Root of the shared ringos toolchain package")

ringos_generate_installed_toolchain_bundle(
  ${RINGOS_TARGET_ARCH}
  RINGOS_INSTALLED_TOOLCHAIN_TARGET
  RINGOS_INSTALLED_TOOLCHAIN_BUNDLE_ROOT
  RINGOS_INSTALLED_TOOLCHAIN_FILE)

if(TARGET ${RINGOS_INSTALLED_TOOLCHAIN_TARGET})
  add_custom_target(ringos_installed_toolchain DEPENDS ${RINGOS_INSTALLED_TOOLCHAIN_TARGET})
endif()

if(TARGET ringos_${RINGOS_TARGET_ARCH}_sdk_sysroot)
  add_custom_target(ringos_sdk_sysroot DEPENDS ringos_${RINGOS_TARGET_ARCH}_sdk_sysroot)
endif()
