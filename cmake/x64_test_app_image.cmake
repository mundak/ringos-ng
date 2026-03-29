function(
  ringos_add_embedded_x64_test_app
  dependency_target
  output_format
  output_architecture
  out_object)
  set(options)
  set(oneValueArgs BINARY_STEM SOURCE_PATH)
  cmake_parse_arguments(RINGOS_TEST_APP "${options}" "${oneValueArgs}" "" ${ARGN})

  find_program(RINGOS_LLD_LINK NAMES lld-link lld-link-18 lld-link-17 REQUIRED)

  if(NOT RINGOS_TEST_APP_SOURCE_PATH)
    set(RINGOS_TEST_APP_SOURCE_PATH ${CMAKE_SOURCE_DIR}/arch/x64/test_app_pe64.S)
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_STEM)
    set(RINGOS_TEST_APP_BINARY_STEM ringos_test_app_x64_pe64_image)
  endif()

  set(X64_TEST_APP_SOURCE ${RINGOS_TEST_APP_SOURCE_PATH})
  set(X64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.obj)
  set(X64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM})
  set(X64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.o)

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
            /subsystem:native
            /nodefaultlib
            /fixed
            /dynamicbase:no
            /base:0x400000
            /filealign:4096
            /out:${X64_TEST_APP_IMAGE}
            ${X64_TEST_APP_OBJECT}
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_OBJCOPY}
            -I binary
            -O ${output_format}
            -B ${output_architecture}
          ${RINGOS_TEST_APP_BINARY_STEM}
          ${RINGOS_TEST_APP_BINARY_STEM}.o
    DEPENDS ${X64_TEST_APP_SOURCE}
    BYPRODUCTS ${X64_TEST_APP_OBJECT} ${X64_TEST_APP_IMAGE}
    COMMENT "Building embedded x64 PE64 test app"
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
