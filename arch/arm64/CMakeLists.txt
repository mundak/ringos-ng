set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/cmake/x64_test_app_image.cmake)

ringos_add_embedded_x64_test_app(
  ringos_arm64_test_app
  elf64-littleaarch64
  aarch64
  X64_TEST_APP_IMAGE_OBJECT)

add_executable(ringos_arm64
  boot.S
  debug.cpp
  entry.cpp
  ${X64_TEST_APP_IMAGE_OBJECT}
  user_runtime.cpp
  user_runtime.S
)

add_dependencies(ringos_arm64 ringos_arm64_test_app)

set(ARM64_KERNEL_COMPILE_OPTIONS
  -mgeneral-regs-only
)

target_compile_options(ringos_kernel
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

target_compile_options(ringos_x64_emulator
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

target_compile_options(ringos_x64_win32_emulation
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

target_compile_options(ringos_arm64
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

target_link_options(ringos_arm64
  PRIVATE
    -T ${LINKER_SCRIPT}
)

target_link_libraries(ringos_arm64
  PRIVATE
    ringos_kernel
    ringos_x64_emulator
    ringos_x64_win32_emulation
)

set_target_properties(ringos_arm64
  PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)
