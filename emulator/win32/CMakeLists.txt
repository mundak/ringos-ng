add_library(ringos_x64_win32_emulation OBJECT
  generated/x64_win32_dll_exports.cpp
  x64_win32_emulation.cpp
)

target_include_directories(ringos_x64_win32_emulation
  PUBLIC
    ${CMAKE_SOURCE_DIR}/emulator/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/kernel/include
)