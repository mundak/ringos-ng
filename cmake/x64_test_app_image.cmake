function(
  ringos_add_embedded_x64_test_app
  dependency_target
  output_format
  output_architecture
  out_object)
  include(${CMAKE_SOURCE_DIR}/cmake/ringos_sdk_sysroot.cmake)
  include(${CMAKE_SOURCE_DIR}/cmake/ringos_installed_toolchain.cmake)

  set(options)
  set(oneValueArgs BINARY_STEM SOURCE_PATH PROJECT_PATH PROJECT_TARGET PROJECT_OUTPUT_NAME)
  cmake_parse_arguments(RINGOS_TEST_APP "${options}" "${oneValueArgs}" "" ${ARGN})

  find_program(RINGOS_LLD_LINK NAMES lld-link lld-link-18 lld-link-17 REQUIRED)

  if(RINGOS_TEST_APP_SOURCE_PATH AND RINGOS_TEST_APP_PROJECT_PATH)
    message(FATAL_ERROR "Specify either SOURCE_PATH or PROJECT_PATH for an embedded x64 test app, not both.")
  endif()

  if(NOT RINGOS_TEST_APP_SOURCE_PATH AND NOT RINGOS_TEST_APP_PROJECT_PATH)
    set(RINGOS_TEST_APP_SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/test_app.c)
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_STEM)
    set(RINGOS_TEST_APP_BINARY_STEM ringos_test_app_x64_pe64_image)
  endif()

  if(RINGOS_TEST_APP_PROJECT_PATH)
    if(NOT RINGOS_TEST_APP_PROJECT_TARGET)
      set(RINGOS_TEST_APP_PROJECT_TARGET hello_world)
    endif()

    if(NOT RINGOS_TEST_APP_PROJECT_OUTPUT_NAME)
      set(RINGOS_TEST_APP_PROJECT_OUTPUT_NAME ${RINGOS_TEST_APP_PROJECT_TARGET})
    endif()
  else()
    get_filename_component(RINGOS_TEST_APP_SOURCE_EXTENSION ${RINGOS_TEST_APP_SOURCE_PATH} EXT)
    set(X64_TEST_APP_SOURCE ${RINGOS_TEST_APP_SOURCE_PATH})
  endif()

  set(X64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.obj)
  set(X64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM})
  set(X64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.o)
  set(X64_TEST_APP_WINDOWS_EXE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.exe)
  set(X64_TEST_APP_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_build)

  ringos_add_sdk_sysroot(
    x64
    RINGOS_SDK_SYSROOT_TARGET
    RINGOS_SDK_TARGET_TRIPLE
    RINGOS_SDK_SYSROOT_DIR
    RINGOS_SDK_SYSROOT_INCLUDE_DIR
    RINGOS_SDK_SYSROOT_LIB_DIR
    RINGOS_SDK_SYSROOT_LIBRARY
    RINGOS_SDK_COMPILE_CONFIG
    RINGOS_SDK_LINK_CONFIG)

  ringos_generate_installed_toolchain_bundle(
    x64
    RINGOS_INSTALLED_TOOLCHAIN_TARGET
    RINGOS_INSTALLED_TOOLCHAIN_ROOT
    RINGOS_INSTALLED_TOOLCHAIN_FILE)

  if(RINGOS_TEST_APP_PROJECT_PATH)
    file(GLOB_RECURSE X64_TEST_APP_PROJECT_INPUTS CONFIGURE_DEPENDS ${RINGOS_TEST_APP_PROJECT_PATH}/*)

    set(X64_TEST_APP_WINDOWS_EXE ${X64_TEST_APP_BUILD_DIR}/${RINGOS_TEST_APP_PROJECT_OUTPUT_NAME}.exe)
    set(X64_TEST_APP_CONFIGURE_COMMAND
      COMMAND ${CMAKE_COMMAND}
              -S ${RINGOS_TEST_APP_PROJECT_PATH}
              -B ${X64_TEST_APP_BUILD_DIR}
              -G ${CMAKE_GENERATOR}
              -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
              -DCMAKE_TOOLCHAIN_FILE=${RINGOS_INSTALLED_TOOLCHAIN_FILE})

    if(CMAKE_MAKE_PROGRAM)
      list(APPEND X64_TEST_APP_CONFIGURE_COMMAND -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
    endif()

    add_custom_command(
      OUTPUT ${X64_TEST_APP_IMAGE_OBJECT}
      ${X64_TEST_APP_CONFIGURE_COMMAND}
      COMMAND ${CMAKE_COMMAND}
              --build ${X64_TEST_APP_BUILD_DIR}
              --target ${RINGOS_TEST_APP_PROJECT_TARGET}
      COMMAND ${CMAKE_COMMAND} -E copy ${X64_TEST_APP_WINDOWS_EXE} ${X64_TEST_APP_IMAGE}
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${CMAKE_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS
        ${RINGOS_INSTALLED_TOOLCHAIN_TARGET}
        ${RINGOS_INSTALLED_TOOLCHAIN_FILE}
        ${X64_TEST_APP_PROJECT_INPUTS}
      BYPRODUCTS
        ${X64_TEST_APP_IMAGE}
        ${X64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded x64 PE64 test app against the installed ringos toolchain bundle"
      VERBATIM
    )
  elseif(RINGOS_TEST_APP_SOURCE_EXTENSION STREQUAL ".c")
    add_custom_command(
      OUTPUT ${X64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_C_COMPILER}
              --config=${RINGOS_SDK_COMPILE_CONFIG}
              --config=${RINGOS_SDK_LINK_CONFIG}
              -O2
              -Wall
              -Wextra
              -Wpedantic
              ${X64_TEST_APP_SOURCE}
              -o ${X64_TEST_APP_WINDOWS_EXE}
      COMMAND ${CMAKE_COMMAND} -E copy ${X64_TEST_APP_WINDOWS_EXE} ${X64_TEST_APP_IMAGE}
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${CMAKE_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS
        ${RINGOS_SDK_SYSROOT_TARGET}
        ${RINGOS_SDK_SYSROOT_LIBRARY}
        ${RINGOS_SDK_COMPILE_CONFIG}
        ${RINGOS_SDK_LINK_CONFIG}
        ${X64_TEST_APP_SOURCE}
      BYPRODUCTS
        ${X64_TEST_APP_IMAGE}
        ${X64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded x64 PE64 test app against the staged ringos sysroot"
      VERBATIM
    )
  else()
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
  endif()

  add_custom_target(${dependency_target} DEPENDS ${X64_TEST_APP_IMAGE_OBJECT})

  set_source_files_properties(${X64_TEST_APP_IMAGE_OBJECT}
    PROPERTIES
      EXTERNAL_OBJECT TRUE
      GENERATED TRUE
  )

  set(${out_object} ${X64_TEST_APP_IMAGE_OBJECT} PARENT_SCOPE)
endfunction()
