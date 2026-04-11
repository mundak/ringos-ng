set(LINKER_SCRIPT $ { CMAKE_CURRENT_SOURCE_DIR } / linker.ld)

  include($ { CMAKE_SOURCE_DIR } / tests / common / x64_test_app_image.cmake)

    set(
      RINGOS_X64_HELLO_WORLD_TEST_APP_ARGS PROJECT_PATH $ { CMAKE_SOURCE_DIR } / user / samples
      / hello_world PROJECT_TARGET hello_world)

      if (DEFINED RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY AND NOT RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY STREQUAL
          "") set(RINGOS_X64_HELLO_WORLD_TEST_APP_ARGS BINARY_PATH $ { RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY }) endif()

        set(
          RINGOS_X64_HELLO_WORLD_CPP_TEST_APP_ARGS PROJECT_PATH $ { CMAKE_SOURCE_DIR } / user / samples
          / hello_world_cpp PROJECT_TARGET hello_world_cpp)

          if (DEFINED RINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY AND NOT
                RINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY STREQUAL "")
            set(RINGOS_X64_HELLO_WORLD_CPP_TEST_APP_ARGS BINARY_PATH $ { RINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY })
              endif()

                ringos_add_embedded_x64_test_app(
                  ringos_x64_hello_world_test_app elf64 - x86 - 64 i386 : x86
                  - 64 X64_HELLO_WORLD_IMAGE_OBJECT BINARY_STEM ringos_hello_world_x64_pe64_image
                    $ { RINGOS_X64_HELLO_WORLD_TEST_APP_ARGS })

                  ringos_add_embedded_x64_test_app(
                    ringos_x64_hello_world_cpp_test_app elf64 - x86 - 64 i386 : x86
                    - 64 X64_HELLO_WORLD_CPP_IMAGE_OBJECT BINARY_STEM ringos_hello_world_cpp_x64_pe64_image
                      $ { RINGOS_X64_HELLO_WORLD_CPP_TEST_APP_ARGS })

                    function(ringos_add_x64_kernel_target target_name client_dependency_target client_image_object
                               client_runtime_mode) add_executable($ {
                      target_name } boot.S debug.cpp entry.cpp machine.cpp $ {
                      client_image_object } user_runtime.cpp user_runtime.S)

                      target_include_directories(
                        $ { target_name } PRIVATE $ { CMAKE_SOURCE_DIR } / kernel $ { CMAKE_SOURCE_DIR } / user / sdk
                        / include)

                        add_dependencies($ { target_name } $ { client_dependency_target })

                          target_compile_options($ { target_name } PRIVATE - m64 - mno - red - zone)

                            target_link_options(
                              $ { target_name } PRIVATE - T $ { LINKER_SCRIPT } --defsym
                              = _binary_ringos_test_app_image_start = _binary_$ { client_runtime_mode } _start-- defsym
                              = _binary_ringos_test_app_image_end = _binary_$ { client_runtime_mode } _end)

                              target_link_libraries($ {
                                target_name } PRIVATE ringos_kernel ringos_x64_emulator ringos_x64_win32_emulation)

                                set_target_properties($ { target_name } PROPERTIES LINK_DEPENDS $ { LINKER_SCRIPT })

#QEMU's Multiboot loader only accepts 32-bit ELF images. Save the original
# 64 - bit ELF(useful for GDB debugging) and convert the primary output to a
# 32 - bit ELF that QEMU can boot.
                                  add_custom_command(
                                    TARGET $ { target_name } POST_BUILD COMMAND $ { CMAKE_COMMAND } - E copy $
                                    < TARGET_FILE : $ { target_name } > $ < TARGET_FILE_DIR : $ { target_name } >
                                    / $ { target_name }.elf64 COMMAND $ { CMAKE_OBJCOPY } - O elf32 - i386 $
                                    < TARGET_FILE_DIR : $ { target_name } > / $ { target_name }.elf64 $
                                    < TARGET_FILE : $ { target_name } > COMMENT
                                    "Converting to Multiboot-compatible 32-bit ELF (original saved as "
                                    "${target_name}.elf64)") endfunction()

                                    ringos_add_x64_kernel_target(ringos_x64 ringos_x64_hello_world_test_app $ {
                                      X64_HELLO_WORLD_IMAGE_OBJECT } ringos_hello_world_x64_pe64_image)

                                      ringos_add_x64_kernel_target(
                                        ringos_x64_hello_world_cpp ringos_x64_hello_world_cpp_test_app $ {
                                          X64_HELLO_WORLD_CPP_IMAGE_OBJECT } ringos_hello_world_cpp_x64_pe64_image)
