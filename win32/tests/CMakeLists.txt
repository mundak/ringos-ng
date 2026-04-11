find_program(RINGOS_HOST_CXX NAMES clang++ g++ c++ REQUIRED)

include(${CMAKE_SOURCE_DIR}/tests/common/x64_test_app_image.cmake)

ringos_add_embedded_x64_test_app(
  ringos_x64_win32_import_test_app
  elf64-x86-64
  i386:x86-64
  RINGOS_X64_WIN32_IMPORT_TEST_APP_IMAGE_OBJECT
  BINARY_STEM ringos_win32_import_test_app_x64_pe64_image
  SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/test_app_pe64.S
)

set(RINGOS_X64_WIN32_TEST_BINARY ${CMAKE_BINARY_DIR}/x64_win32_loader_unit_tests)

set(RINGOS_X64_WIN32_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/x64_win32_loader_tests.cpp
  ${CMAKE_SOURCE_DIR}/win32/generated/x64_win32_dll_exports.cpp
  ${CMAKE_SOURCE_DIR}/win32/pe_image.cpp
  ${CMAKE_SOURCE_DIR}/win32/x64_pe64_image.cpp
  ${CMAKE_SOURCE_DIR}/win32/x64_win32_emulation.cpp
)

add_custom_command(
  OUTPUT ${RINGOS_X64_WIN32_TEST_BINARY}
  COMMAND ${RINGOS_HOST_CXX}
          -std=c++20
          -Wall
          -Wextra
          -Wpedantic
          -I ${CMAKE_SOURCE_DIR}/win32/include
          -I ${CMAKE_SOURCE_DIR}/win32/generated
          -I ${CMAKE_SOURCE_DIR}/kernel
          ${RINGOS_X64_WIN32_TEST_SOURCES}
          ${RINGOS_X64_WIN32_IMPORT_TEST_APP_IMAGE_OBJECT}
          -o ${RINGOS_X64_WIN32_TEST_BINARY}
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_app_pe64.S
    ${CMAKE_CURRENT_SOURCE_DIR}/x64_win32_loader_tests.cpp
    ${CMAKE_SOURCE_DIR}/kernel/user_space.h
    ${CMAKE_SOURCE_DIR}/win32/definitions/kernel32.yaml
    ${CMAKE_SOURCE_DIR}/win32/generate_win32_emulation.py
    ${CMAKE_SOURCE_DIR}/win32/generated/x64_win32_dll_exports.h
    ${CMAKE_SOURCE_DIR}/win32/include/pe_image.h
    ${CMAKE_SOURCE_DIR}/win32/include/x64_pe64_image.h
    ${CMAKE_SOURCE_DIR}/win32/include/x64_win32_emulation.h
    ${RINGOS_X64_WIN32_TEST_SOURCES}
    ${RINGOS_X64_WIN32_IMPORT_TEST_APP_IMAGE_OBJECT}
  COMMENT "Building x64 Win32 loader unit tests"
  VERBATIM
)

add_custom_target(ringos_x64_win32_loader_unit_tests ALL DEPENDS ${RINGOS_X64_WIN32_TEST_BINARY})

add_test(
  NAME x64_win32_loader_unit
  COMMAND ${RINGOS_X64_WIN32_TEST_BINARY}
)

set_tests_properties(
  x64_win32_loader_unit
  PROPERTIES
    TIMEOUT 5
)
