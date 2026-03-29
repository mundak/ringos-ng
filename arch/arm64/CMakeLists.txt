set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/cmake/arm64_test_app_image.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/x64_test_app_image.cmake)

ringos_add_embedded_arm64_test_app(
  ringos_arm64_test_app
  elf64-littleaarch64
  aarch64
  ARM64_TEST_APP_IMAGE_OBJECT)

ringos_add_embedded_x64_test_app(
  ringos_arm64_x64_test_app
  elf64-littleaarch64
  aarch64
  X64_TEST_APP_IMAGE_OBJECT)

ringos_add_embedded_arm64_test_app(
  ringos_arm64_ansi_c_test_app
  elf64-littleaarch64
  aarch64
  ARM64_ANSI_C_TEST_APP_IMAGE_OBJECT
  BINARY_STEM ringos_ansi_c_test_app_arm64_pe64_image
  SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world.c)

set(ARM64_KERNEL_COMPILE_OPTIONS
  -mgeneral-regs-only
)

function(ringos_add_arm64_kernel_target target_name dependency_target image_object runtime_mode)
  add_executable(${target_name}
    boot.S
    debug.cpp
    entry.cpp
    ${image_object}
    user_runtime.cpp
    user_runtime.S
  )

  add_dependencies(${target_name} ${dependency_target})
  target_compile_options(${target_name}
    PRIVATE
      ${ARM64_KERNEL_COMPILE_OPTIONS}
  )
  target_link_options(${target_name}
    PRIVATE
      -T ${LINKER_SCRIPT}
      --defsym=_binary_ringos_test_app_image_start=_binary_${runtime_mode}_start
      --defsym=_binary_ringos_test_app_image_end=_binary_${runtime_mode}_end
  )
  target_link_libraries(${target_name}
    PRIVATE
      ringos_kernel
      ringos_x64_emulator
      ringos_x64_win32_emulation
  )
  set_target_properties(${target_name}
    PROPERTIES
      LINK_DEPENDS ${LINKER_SCRIPT}
  )
endfunction()

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

ringos_add_arm64_kernel_target(
  ringos_arm64
  ringos_arm64_test_app
  ${ARM64_TEST_APP_IMAGE_OBJECT}
  ringos_test_app_arm64_pe64_image)

ringos_add_arm64_kernel_target(
  ringos_arm64_ansi_c
  ringos_arm64_ansi_c_test_app
  ${ARM64_ANSI_C_TEST_APP_IMAGE_OBJECT}
  ringos_ansi_c_test_app_arm64_pe64_image)

ringos_add_arm64_kernel_target(
  ringos_arm64_x64_emulator
  ringos_arm64_x64_test_app
  ${X64_TEST_APP_IMAGE_OBJECT}
  ringos_test_app_x64_pe64_image)
