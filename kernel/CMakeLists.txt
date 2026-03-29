# Shared kernel code — populated in Phase 2 and later
add_library(ringos_kernel OBJECT
  kernel.cpp
  debug.cpp
  kernel_object.cpp
  process.cpp
  thread.cpp
  channel.cpp
  shared_memory_object.cpp
  memory.cpp
  panic.cpp
  user_runtime.cpp
)

target_include_directories(ringos_kernel
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/emulator/include
)

if(RINGOS_TARGET_ARCH STREQUAL "x64")
  target_compile_options(ringos_kernel
    PRIVATE
      -mno-red-zone
  )
endif()
