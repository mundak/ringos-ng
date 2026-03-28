set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

find_program(RINGOS_LLD_LINK NAMES lld-link lld-link-18 lld-link-17 REQUIRED)

set(X64_TEST_APP_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_app_pe64.cpp)
set(X64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image.obj)
set(X64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image)
set(X64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image.o)

add_custom_command(
  OUTPUT ${X64_TEST_APP_IMAGE_OBJECT}
    COMMAND ${CMAKE_CXX_COMPILER}
          --target=x86_64-pc-windows-msvc
          -ffreestanding
          -fno-builtin
      -fno-exceptions
      -fno-rtti
          -fno-stack-protector
          -fno-asynchronous-unwind-tables
          -fno-unwind-tables
          -Wall
          -Wextra
          -Wpedantic
          -c ${X64_TEST_APP_SOURCE}
          -o ${X64_TEST_APP_OBJECT}
  COMMAND ${RINGOS_LLD_LINK}
          /machine:x64
          /entry:user_start
          /subsystem:native
          /nodefaultlib
          /fixed
          /dynamicbase:no
          /base:0x20000000
          /filealign:4096
          /out:${X64_TEST_APP_IMAGE}
          ${X64_TEST_APP_OBJECT}
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_OBJCOPY}
          -I binary
          -O elf64-x86-64
          -B i386:x86-64
          ringos_test_app_x64_pe64_image
          ringos_test_app_x64_pe64_image.o
  DEPENDS ${X64_TEST_APP_SOURCE}
  BYPRODUCTS ${X64_TEST_APP_OBJECT} ${X64_TEST_APP_IMAGE}
  COMMENT "Building embedded x64 PE64 test app"
  VERBATIM
)

add_custom_target(ringos_x64_test_app DEPENDS ${X64_TEST_APP_IMAGE_OBJECT})

set_source_files_properties(${X64_TEST_APP_IMAGE_OBJECT}
  PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED TRUE
)

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
