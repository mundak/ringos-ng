set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/cmake/x64_test_app_image.cmake)

ringos_add_embedded_x64_test_app(
  ringos_x64_test_app
  elf64-x86-64
  i386:x86-64
  X64_TEST_APP_IMAGE_OBJECT)

add_executable(ringos_x64
  boot.S
  debug.cpp
  entry.cpp
  ${X64_TEST_APP_IMAGE_OBJECT}
  user_runtime.cpp
  user_runtime.S
)

add_dependencies(ringos_x64 ringos_x64_test_app)

target_compile_options(ringos_x64
  PRIVATE
    -m64
    -mno-red-zone
)

target_link_options(ringos_x64
  PRIVATE
    -T ${LINKER_SCRIPT}
)

target_link_libraries(ringos_x64
  PRIVATE
    ringos_kernel
    ringos_x64_emulator
)

set_target_properties(ringos_x64
  PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# QEMU's Multiboot loader only accepts 32-bit ELF images. Save the original
# 64-bit ELF (useful for GDB debugging) and convert the primary output to a
# 32-bit ELF that QEMU can boot.
add_custom_command(TARGET ringos_x64 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ringos_x64> $<TARGET_FILE_DIR:ringos_x64>/ringos_x64.elf64
  COMMAND ${CMAKE_OBJCOPY} -O elf32-i386 $<TARGET_FILE_DIR:ringos_x64>/ringos_x64.elf64 $<TARGET_FILE:ringos_x64>
  COMMENT "Converting to Multiboot-compatible 32-bit ELF (original saved as ringos_x64.elf64)"
)
