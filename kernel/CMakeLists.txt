# Shared kernel code — populated in Phase 2 and later
add_library(ringos_kernel OBJECT
  kernel.cpp
  debug.cpp
  panic.cpp
)

target_include_directories(ringos_kernel
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
