function(
  ringos_add_embedded_arm64_test_app
  dependency_target
  output_format
  output_architecture
  out_object)
  include(${CMAKE_SOURCE_DIR}/tools/toolchain/ringos_sdk_sysroot.cmake)
  include(${CMAKE_SOURCE_DIR}/tools/toolchain/ringos_installed_toolchain.cmake)
  include(${CMAKE_SOURCE_DIR}/tools/toolchain/ringos-llvm-root.cmake)

  set(options)
  set(oneValueArgs BINARY_STEM SOURCE_PATH PROJECT_PATH PROJECT_TARGET PROJECT_OUTPUT_NAME)
  cmake_parse_arguments(RINGOS_TEST_APP "${options}" "${oneValueArgs}" "" ${ARGN})

  ringos_find_llvm_tool(lld-link RINGOS_LLD_LINK lld-link lld-link-18 lld-link-17)

  if(RINGOS_TEST_APP_SOURCE_PATH AND RINGOS_TEST_APP_PROJECT_PATH)
    message(FATAL_ERROR "Specify either SOURCE_PATH or PROJECT_PATH for an embedded arm64 test app, not both.")
  endif()

  if(NOT RINGOS_TEST_APP_SOURCE_PATH AND NOT RINGOS_TEST_APP_PROJECT_PATH)
    set(RINGOS_TEST_APP_SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world/hello_world.c)
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_STEM)
    set(RINGOS_TEST_APP_BINARY_STEM ringos_test_app_arm64_pe64_image)
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
    set(ARM64_TEST_APP_SOURCE ${RINGOS_TEST_APP_SOURCE_PATH})
  endif()

  set(ARM64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.obj)
  set(ARM64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM})
  set(ARM64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.o)
  set(ARM64_TEST_APP_WINDOWS_EXE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.exe)
  set(ARM64_TEST_APP_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_build)

  ringos_add_sdk_sysroot(
    arm64
    RINGOS_SDK_SYSROOT_TARGET
    RINGOS_SDK_TARGET_TRIPLE
    RINGOS_SDK_SYSROOT_DIR
    RINGOS_SDK_SYSROOT_INCLUDE_DIR
    RINGOS_SDK_SYSROOT_LIB_DIR
    RINGOS_SDK_SYSROOT_LIBRARY
    RINGOS_SDK_COMPILE_CONFIG
    RINGOS_SDK_LINK_CONFIG)

  ringos_generate_installed_toolchain_bundle(
    arm64
    RINGOS_INSTALLED_TOOLCHAIN_TARGET
    RINGOS_INSTALLED_TOOLCHAIN_ROOT
    RINGOS_INSTALLED_TOOLCHAIN_FILE)

  if(RINGOS_TEST_APP_PROJECT_PATH)
    file(GLOB_RECURSE ARM64_TEST_APP_PROJECT_INPUTS CONFIGURE_DEPENDS ${RINGOS_TEST_APP_PROJECT_PATH}/*)

    set(ARM64_TEST_APP_WINDOWS_EXE ${ARM64_TEST_APP_BUILD_DIR}/${RINGOS_TEST_APP_PROJECT_OUTPUT_NAME}.exe)
    set(ARM64_TEST_APP_CONFIGURE_COMMAND
      COMMAND ${CMAKE_COMMAND}
              -S ${RINGOS_TEST_APP_PROJECT_PATH}
              -B ${ARM64_TEST_APP_BUILD_DIR}
              -G ${CMAKE_GENERATOR}
              -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
              -DCMAKE_TOOLCHAIN_FILE=${RINGOS_INSTALLED_TOOLCHAIN_FILE})

    if(CMAKE_MAKE_PROGRAM)
      list(APPEND ARM64_TEST_APP_CONFIGURE_COMMAND -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
    endif()

    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      ${ARM64_TEST_APP_CONFIGURE_COMMAND}
      COMMAND ${CMAKE_COMMAND}
              --build ${ARM64_TEST_APP_BUILD_DIR}
              --target ${RINGOS_TEST_APP_PROJECT_TARGET}
      COMMAND ${CMAKE_COMMAND} -E copy ${ARM64_TEST_APP_WINDOWS_EXE} ${ARM64_TEST_APP_IMAGE}
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
        ${ARM64_TEST_APP_PROJECT_INPUTS}
      BYPRODUCTS
        ${ARM64_TEST_APP_IMAGE}
        ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded arm64 PE64 test app against the installed ringos toolchain bundle"
      VERBATIM)
  elseif(RINGOS_TEST_APP_SOURCE_EXTENSION STREQUAL ".c")
    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_C_COMPILER}
              --config=${RINGOS_SDK_COMPILE_CONFIG}
              --config=${RINGOS_SDK_LINK_CONFIG}
              -O2
              -Wall
              -Wextra
              -Wpedantic
              ${ARM64_TEST_APP_SOURCE}
              -o ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMAND ${CMAKE_COMMAND} -E copy ${ARM64_TEST_APP_WINDOWS_EXE} ${ARM64_TEST_APP_IMAGE}
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
        ${ARM64_TEST_APP_SOURCE}
      BYPRODUCTS
        ${ARM64_TEST_APP_IMAGE}
        ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded arm64 PE64 test app against the staged ringos sysroot"
      VERBATIM)
  elseif(RINGOS_TEST_APP_SOURCE_EXTENSION STREQUAL ".cpp")
    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_CXX_COMPILER}
              --config=${RINGOS_SDK_COMPILE_CONFIG}
              --config=${RINGOS_SDK_LINK_CONFIG}
              -O2
              -Wall
              -Wextra
              -Wpedantic
              ${ARM64_TEST_APP_SOURCE}
              -o ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMAND ${CMAKE_COMMAND} -E copy ${ARM64_TEST_APP_WINDOWS_EXE} ${ARM64_TEST_APP_IMAGE}
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
        ${ARM64_TEST_APP_SOURCE}
      BYPRODUCTS
        ${ARM64_TEST_APP_IMAGE}
        ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded arm64 PE64 C++ test app against the staged ringos sysroot"
      VERBATIM)
  else()
    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_ASM_COMPILER}
              --target=aarch64-pc-windows-msvc
              -ffreestanding
              -fno-stack-protector
              -c ${ARM64_TEST_APP_SOURCE}
              -o ${ARM64_TEST_APP_OBJECT}
      COMMAND ${RINGOS_LLD_LINK}
              /machine:arm64
              /entry:user_start
              /subsystem:native
              /nodefaultlib
              /fixed:no
              /base:0x400000
              /filealign:4096
              /out:${ARM64_TEST_APP_IMAGE}
              ${ARM64_TEST_APP_OBJECT}
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${CMAKE_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS ${ARM64_TEST_APP_SOURCE}
      BYPRODUCTS ${ARM64_TEST_APP_OBJECT} ${ARM64_TEST_APP_IMAGE}
      COMMENT "Building embedded arm64 PE64 test app"
      VERBATIM)
  endif()

  add_custom_target(${dependency_target} DEPENDS ${ARM64_TEST_APP_IMAGE_OBJECT})

  set_source_files_properties(${ARM64_TEST_APP_IMAGE_OBJECT}
    PROPERTIES
      EXTERNAL_OBJECT TRUE
      GENERATED TRUE)

  set(${out_object} ${ARM64_TEST_APP_IMAGE_OBJECT} PARENT_SCOPE)
endfunction()
