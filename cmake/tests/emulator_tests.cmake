find_program(RINGOS_HOST_CXX NAMES clang++ g++ c++ REQUIRED)

set(RINGOS_X64_EMULATOR_TEST_BINARY ${CMAKE_BINARY_DIR}/x64_emulator_unit_tests)

set(RINGOS_X64_EMULATOR_TEST_SOURCES
  ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_alu_tests.cpp
  ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_data_movement_tests.cpp
  ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_system_tests.cpp
  ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_stack_and_control_flow_tests.cpp
  ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_test_harness.cpp
  ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_unit_tests.cpp
)

set(RINGOS_X64_EMULATOR_IMPLEMENTATION_SOURCES
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_alu_instructions.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_control_flow_instructions.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_data_movement_instructions.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_decoder.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_emulator.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_execution_context.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_instruction_dispatch.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_interpreter.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_stack_instructions.cpp
  ${CMAKE_SOURCE_DIR}/emulator/x64/x64_system_instructions.cpp
)

add_custom_command(
  OUTPUT ${RINGOS_X64_EMULATOR_TEST_BINARY}
  COMMAND ${RINGOS_HOST_CXX}
          -std=c++20
          -Wall
          -Wextra
          -Wpedantic
          -I ${CMAKE_SOURCE_DIR}/emulator/include
          -I ${CMAKE_SOURCE_DIR}/kernel/include
          ${RINGOS_X64_EMULATOR_TEST_SOURCES}
          ${RINGOS_X64_EMULATOR_IMPLEMENTATION_SOURCES}
          -o ${RINGOS_X64_EMULATOR_TEST_BINARY}
  DEPENDS
    ${RINGOS_X64_EMULATOR_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/emulator/include/x64_emulator.h
    ${CMAKE_SOURCE_DIR}/kernel/include/user_space.h
    ${CMAKE_SOURCE_DIR}/emulator/tests/x64_emulator_test_harness.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_decoder.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_execution_context.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_instruction.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_instruction_dispatch.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_instruction_handlers.h
    ${CMAKE_SOURCE_DIR}/emulator/x64/x64_interpreter.h
    ${RINGOS_X64_EMULATOR_IMPLEMENTATION_SOURCES}
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
