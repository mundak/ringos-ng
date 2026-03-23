# Shared kernel code — populated in Phase 2 and later
add_library(ringos_kernel OBJECT
  kernel.cpp
  console.cpp
  panic.cpp
  format.cpp
)

target_include_directories(ringos_kernel
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
