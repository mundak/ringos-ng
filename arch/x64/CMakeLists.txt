set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/cmake/x64_test_app_image.cmake)

ringos_add_embedded_x64_test_app(
  ringos_x64_test_app
  elf64-x86-64
  i386:x86-64
  X64_TEST_APP_IMAGE_OBJECT)

ringos_add_embedded_x64_test_app(
  ringos_x64_ansi_c_test_app
  elf64-x86-64
  i386:x86-64
  X64_ANSI_C_TEST_APP_IMAGE_OBJECT
  BINARY_STEM ringos_ansi_c_test_app_x64_pe64_image
  SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world.c)

function(ringos_add_x64_kernel_target target_name dependency_target image_object runtime_mode)
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
      -m64
      -mno-red-zone
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

  # QEMU's Multiboot loader only accepts 32-bit ELF images. Save the original
  # 64-bit ELF (useful for GDB debugging) and convert the primary output to a
  # 32-bit ELF that QEMU can boot.
  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target_name}> $<TARGET_FILE_DIR:${target_name}>/${target_name}.elf64
    COMMAND ${CMAKE_OBJCOPY} -O elf32-i386 $<TARGET_FILE_DIR:${target_name}>/${target_name}.elf64 $<TARGET_FILE:${target_name}>
    COMMENT "Converting to Multiboot-compatible 32-bit ELF (original saved as ${target_name}.elf64)"
  )
endfunction()

ringos_add_x64_kernel_target(
  ringos_x64
  ringos_x64_test_app
  ${X64_TEST_APP_IMAGE_OBJECT}
  ringos_test_app_x64_pe64_image)

ringos_add_x64_kernel_target(
  ringos_x64_ansi_c
  ringos_x64_ansi_c_test_app
  ${X64_ANSI_C_TEST_APP_IMAGE_OBJECT}
  ringos_ansi_c_test_app_x64_pe64_image)
