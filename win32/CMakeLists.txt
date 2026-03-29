add_library(ringos_x64_win32_emulation OBJECT
  generated/x64_win32_dll_exports.cpp
  x64_pe64_image.cpp
  x64_win32_emulation.cpp
  x64_windows_compat.cpp
)

target_include_directories(ringos_x64_win32_emulation
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/kernel/include
)
