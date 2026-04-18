cmake_minimum_required(VERSION 3.27)

if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR
    "RINGOS_TARGET_ARCH is not set. Configure with -DRINGOS_TARGET_ARCH=<x64|arm64> "
    "after extracting the RingOS toolchain bundle (for example, "
    "cmake -S . -B build/x64-debug -G Ninja -DRINGOS_TARGET_ARCH=x64).")
endif()

if(NOT RINGOS_TARGET_ARCH STREQUAL "x64" AND NOT RINGOS_TARGET_ARCH STREQUAL "arm64")
  message(FATAL_ERROR "RINGOS_TARGET_ARCH must be 'x64' or 'arm64'. Got: ${RINGOS_TARGET_ARCH}")
endif()

if(DEFINED RINGOS_INSTALLED_TOOLCHAIN_FILE AND NOT RINGOS_INSTALLED_TOOLCHAIN_FILE STREQUAL "")
  set(ringos_installed_toolchain_file "${RINGOS_INSTALLED_TOOLCHAIN_FILE}")
elseif(DEFINED ENV{RINGOS_INSTALLED_TOOLCHAIN_FILE} AND NOT "$ENV{RINGOS_INSTALLED_TOOLCHAIN_FILE}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{RINGOS_INSTALLED_TOOLCHAIN_FILE}" ringos_installed_toolchain_file)
else()
  get_filename_component(
    ringos_installed_toolchain_file
    "${CMAKE_CURRENT_LIST_DIR}/build/toolchain/cmake/ringos-toolchain.cmake"
    ABSOLUTE)
endif()

set(RINGOS_INSTALLED_TOOLCHAIN_FILE "${ringos_installed_toolchain_file}"
  CACHE FILEPATH "RingOS installed user-space toolchain file")

if(NOT EXISTS "${RINGOS_INSTALLED_TOOLCHAIN_FILE}")
  message(FATAL_ERROR
    "RINGOS_INSTALLED_TOOLCHAIN_FILE does not exist: ${RINGOS_INSTALLED_TOOLCHAIN_FILE}. "
    "Extract the RingOS toolchain bundle into build/toolchain or set "
    "-DRINGOS_INSTALLED_TOOLCHAIN_FILE=<toolchain>/cmake/ringos-toolchain.cmake.")
endif()

if(RINGOS_TARGET_ARCH STREQUAL "x64")
  set(RINGOS_KERNEL_SYSTEM_PROCESSOR x86_64)
  set(RINGOS_KERNEL_TARGET_TRIPLE x86_64-none-elf)
else()
  set(RINGOS_KERNEL_SYSTEM_PROCESSOR aarch64)
  set(RINGOS_KERNEL_TARGET_TRIPLE aarch64-none-elf)
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ${RINGOS_KERNEL_SYSTEM_PROCESSOR})

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_OBJCOPY llvm-objcopy)

set(CMAKE_C_COMPILER_TARGET ${RINGOS_KERNEL_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${RINGOS_KERNEL_TARGET_TRIPLE})
set(CMAKE_ASM_COMPILER_TARGET ${RINGOS_KERNEL_TARGET_TRIPLE})

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(ringos LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_LINK_EXECUTABLE "ld.lld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "ld.lld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_ASM_LINK_EXECUTABLE "ld.lld <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

add_compile_options(-ffreestanding -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)

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
