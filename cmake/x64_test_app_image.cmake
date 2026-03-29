function(
  ringos_add_embedded_x64_test_app
  dependency_target
  output_format
  output_architecture
  out_object)
  find_program(RINGOS_LLD_LINK NAMES lld-link lld-link-18 lld-link-17 REQUIRED)

  set(X64_TEST_APP_SOURCE ${CMAKE_SOURCE_DIR}/arch/x64/test_app_pe64.S)
  set(X64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image.obj)
  set(X64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image)
  set(X64_TEST_APP_WINDOWS_EXE ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image.exe)
  set(X64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_test_app_x64_pe64_image.o)

  add_custom_command(
    OUTPUT ${X64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_ASM_COMPILER}
            --target=x86_64-pc-windows-msvc
            -ffreestanding
            -fno-stack-protector
            -c ${X64_TEST_APP_SOURCE}
            -o ${X64_TEST_APP_OBJECT}
    COMMAND ${RINGOS_LLD_LINK}
            /machine:x64
            /entry:user_start
          /subsystem:console
            /nodefaultlib
            /fixed
            /dynamicbase:no
            /base:0x400000
            /filealign:4096
            /out:${X64_TEST_APP_IMAGE}
            ${X64_TEST_APP_OBJECT}
        COMMAND ${CMAKE_COMMAND} -E copy ${X64_TEST_APP_IMAGE} ${X64_TEST_APP_WINDOWS_EXE}
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_OBJCOPY}
            -I binary
            -O ${output_format}
            -B ${output_architecture}
            ringos_test_app_x64_pe64_image
            ringos_test_app_x64_pe64_image.o
    DEPENDS ${X64_TEST_APP_SOURCE}
    BYPRODUCTS ${X64_TEST_APP_OBJECT} ${X64_TEST_APP_IMAGE} ${X64_TEST_APP_WINDOWS_EXE}
    COMMENT "Building embedded x64 PE64 Windows test app"
    VERBATIM
  )

  add_custom_target(${dependency_target} DEPENDS ${X64_TEST_APP_IMAGE_OBJECT})

  set_source_files_properties(${X64_TEST_APP_IMAGE_OBJECT}
    PROPERTIES
      EXTERNAL_OBJECT TRUE
      GENERATED TRUE
  )

  set(${out_object} ${X64_TEST_APP_IMAGE_OBJECT} PARENT_SCOPE)
endfunction()
