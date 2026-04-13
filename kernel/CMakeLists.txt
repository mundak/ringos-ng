# Shared kernel code — populated in Phase 2 and later
add_library(ringos_kernel OBJECT
  kernel.cpp
  machine.cpp
  debug.cpp
  device_memory_object.cpp
  kernel_object.cpp
  process.cpp
  rpc.cpp
  thread.cpp
  shared_memory_object.cpp
  klibc/memory.cpp
  klibc/string.cpp
  panic.cpp
  user_runtime.cpp
)

target_include_directories(ringos_kernel
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/user/sdk/include
  PRIVATE
    ${CMAKE_SOURCE_DIR}/win32/include
)

if(RINGOS_TARGET_ARCH STREQUAL "x64")
  target_compile_options(ringos_kernel
    PRIVATE
      -mno-red-zone
  )
endif()
