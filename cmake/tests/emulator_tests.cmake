find_program(RINGOS_HOST_CXX NAMES clang++ g++ c++ REQUIRED)

set(RINGOS_X64_EMULATOR_TEST_BINARY ${CMAKE_BINARY_DIR}/x64_emulator_unit_tests)

add_custom_command(
  OUTPUT ${RINGOS_X64_EMULATOR_TEST_BINARY}
  COMMAND ${RINGOS_HOST_CXX}
          -std=c++20
          -Wall
          -Wextra
          -Wpedantic
          -I ${CMAKE_SOURCE_DIR}/emulator/include
          -I ${CMAKE_SOURCE_DIR}/kernel/include
          ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_unit_tests.cpp
          ${CMAKE_SOURCE_DIR}/emulator/x64/x64_emulator.cpp
          ${CMAKE_SOURCE_DIR}/emulator/x64/x64_pe64_image.cpp
          -o ${RINGOS_X64_EMULATOR_TEST_BINARY}
  DEPENDS
    ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_unit_tests.cpp
    ${CMAKE_SOURCE_DIR}/emulator/include/x64_emulator.h
    ${CMAKE_SOURCE_DIR}/emulator/include/x64_pe64_image.h
    ${CMAKE_SOURCE_DIR}/kernel/include/user_space.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_emulator.cpp
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_pe64_image.cpp
  COMMENT "Building host-side x64 emulator unit tests"
  VERBATIM
)

add_custom_target(ringos_x64_emulator_unit_tests ALL DEPENDS ${RINGOS_X64_EMULATOR_TEST_BINARY})

add_test(
  NAME x64_emulator_unit
  COMMAND ${RINGOS_X64_EMULATOR_TEST_BINARY}
)

set_tests_properties(
  x64_emulator_unit
  PROPERTIES
    TIMEOUT 5
)
