add_library(ringos_x64_emulator OBJECT
  x64_alu_instructions.cpp
  x64_control_flow_instructions.cpp
  x64_data_movement_instructions.cpp
  x64_decoder.cpp
  x64_emulator.cpp
  x64_execution_context.cpp
  x64_interpreter.cpp
  x64_instruction_dispatch.cpp
  x64_simd_instructions.cpp
  x64_stack_instructions.cpp
  x64_system_instructions.cpp
)

target_include_directories(ringos_x64_emulator
  PUBLIC
    ${CMAKE_SOURCE_DIR}/emulator/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
)
