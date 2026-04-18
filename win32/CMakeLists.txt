add_library(ringos_x64_pe_loader OBJECT
  pe_image.cpp
  x64_pe64_image.cpp
)

target_include_directories(ringos_x64_pe_loader
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
)

add_library(ringos_x64_win32_emulation OBJECT
  generated/x64_win32_dll_exports.cpp
  x64_win32_emulation.cpp
)

target_include_directories(ringos_x64_win32_emulation
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/kernel
)
