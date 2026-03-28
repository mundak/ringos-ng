add_library(ringos_x64_emulator OBJECT
  x64_emulator.cpp
  x64_pe64_image.cpp
)

target_include_directories(ringos_x64_emulator
  PUBLIC
    ${CMAKE_SOURCE_DIR}/emulator/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel/include
)
