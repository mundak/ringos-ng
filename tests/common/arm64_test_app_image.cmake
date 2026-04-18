include(${CMAKE_SOURCE_DIR}/tests/common/test_app_image_common.cmake)

function(
  ringos_add_embedded_arm64_test_app
  dependency_target
  output_format
  output_architecture
  out_object)
  set(options)
  set(oneValueArgs BINARY_STEM BINARY_PATH SOURCE_PATH PROJECT_PATH PROJECT_TARGET PROJECT_OUTPUT_NAME)
  cmake_parse_arguments(RINGOS_TEST_APP "${options}" "${oneValueArgs}" "" ${ARGN})

  if((RINGOS_TEST_APP_SOURCE_PATH AND RINGOS_TEST_APP_PROJECT_PATH)
     OR (RINGOS_TEST_APP_BINARY_PATH AND RINGOS_TEST_APP_SOURCE_PATH)
     OR (RINGOS_TEST_APP_BINARY_PATH AND RINGOS_TEST_APP_PROJECT_PATH))
    message(FATAL_ERROR "Specify exactly one of BINARY_PATH, SOURCE_PATH, or PROJECT_PATH for an embedded arm64 test app.")
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_PATH AND NOT RINGOS_TEST_APP_SOURCE_PATH AND NOT RINGOS_TEST_APP_PROJECT_PATH)
    set(RINGOS_TEST_APP_SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world/hello_world.c)
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_STEM)
    set(RINGOS_TEST_APP_BINARY_STEM ringos_test_app_arm64_pe64_image)
  endif()

  if(RINGOS_TEST_APP_BINARY_PATH)
    get_filename_component(RINGOS_TEST_APP_BINARY_PATH ${RINGOS_TEST_APP_BINARY_PATH} ABSOLUTE)
  elseif(RINGOS_TEST_APP_PROJECT_PATH)
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

  set(ARM64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM})
  set(ARM64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.o)
  set(ARM64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.obj)
  set(ARM64_TEST_APP_WINDOWS_EXE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.exe)
  set(ARM64_TEST_APP_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_build)

  ringos_resolve_bundled_test_app_tools(arm64 ARM64_TEST_APP)

  if(RINGOS_TEST_APP_BINARY_PATH)
    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_COMMAND} -E copy ${RINGOS_TEST_APP_BINARY_PATH} ${ARM64_TEST_APP_IMAGE}
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${ARM64_TEST_APP_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS ${RINGOS_TEST_APP_BINARY_PATH}
      BYPRODUCTS ${ARM64_TEST_APP_IMAGE}
      COMMENT "Embedding prebuilt arm64 PE64 test app image"
      VERBATIM)
  elseif(RINGOS_TEST_APP_PROJECT_PATH)
    file(GLOB_RECURSE ARM64_TEST_APP_PROJECT_INPUTS CONFIGURE_DEPENDS ${RINGOS_TEST_APP_PROJECT_PATH}/*)

    set(ARM64_TEST_APP_WINDOWS_EXE ${ARM64_TEST_APP_BUILD_DIR}/${RINGOS_TEST_APP_PROJECT_OUTPUT_NAME}.exe)
    set(ARM64_TEST_APP_CONFIGURE_COMMAND
      COMMAND ${CMAKE_COMMAND}
              -S ${RINGOS_TEST_APP_PROJECT_PATH}
              -B ${ARM64_TEST_APP_BUILD_DIR}
              -G ${CMAKE_GENERATOR}
              -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
              -DCMAKE_TOOLCHAIN_FILE=${ARM64_TEST_APP_TOOLCHAIN_FILE}
              -DRINGOS_TARGET_ARCH=arm64)

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
              ${ARM64_TEST_APP_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS
        ${ARM64_TEST_APP_TOOLCHAIN_FILE}
        ${ARM64_TEST_APP_PROJECT_INPUTS}
      BYPRODUCTS
        ${ARM64_TEST_APP_IMAGE}
        ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded arm64 PE64 test app against the extracted ringos toolchain bundle"
      VERBATIM)
  elseif(RINGOS_TEST_APP_SOURCE_EXTENSION STREQUAL ".c")
    foreach(required_path
        ${ARM64_TEST_APP_COMPILE_CONFIG}
        ${ARM64_TEST_APP_LINK_CONFIG})
      if(NOT EXISTS ${required_path})
        message(FATAL_ERROR
          "Bundled toolchain input is missing: ${required_path}. "
          "Extract build/toolchain and build/sdk before building embedded arm64 C test apps.")
      endif()
    endforeach()

    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${ARM64_TEST_APP_CLANG}
              --config=${ARM64_TEST_APP_COMPILE_CONFIG}
              --config=${ARM64_TEST_APP_LINK_CONFIG}
              -O2
              -Wall
              -Wextra
              -Wpedantic
              ${ARM64_TEST_APP_SOURCE}
              -o ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMAND ${CMAKE_COMMAND} -E copy ${ARM64_TEST_APP_WINDOWS_EXE} ${ARM64_TEST_APP_IMAGE}
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${ARM64_TEST_APP_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS
        ${ARM64_TEST_APP_COMPILE_CONFIG}
        ${ARM64_TEST_APP_LINK_CONFIG}
        ${ARM64_TEST_APP_SOURCE}
      BYPRODUCTS
        ${ARM64_TEST_APP_IMAGE}
        ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded arm64 PE64 test app against the extracted ringos toolchain bundle"
      VERBATIM)
  elseif(RINGOS_TEST_APP_SOURCE_EXTENSION STREQUAL ".cpp")
    foreach(required_path
        ${ARM64_TEST_APP_COMPILE_CONFIG}
        ${ARM64_TEST_APP_LINK_CONFIG})
      if(NOT EXISTS ${required_path})
        message(FATAL_ERROR
          "Bundled toolchain input is missing: ${required_path}. "
          "Extract build/toolchain and build/sdk before building embedded arm64 C++ test apps.")
      endif()
    endforeach()

    if(NOT EXISTS ${ARM64_TEST_APP_CXX_INCLUDE_DIR})
      message(FATAL_ERROR
        "Bundled C++ include directory is missing: ${ARM64_TEST_APP_CXX_INCLUDE_DIR}. "
        "Extract a toolchain bundle that includes libc++ headers before building embedded arm64 C++ test apps.")
    endif()

    add_custom_command(
      OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${ARM64_TEST_APP_CLANGXX}
              --config=${ARM64_TEST_APP_COMPILE_CONFIG}
              --config=${ARM64_TEST_APP_LINK_CONFIG}
              -O2
              -Wall
              -Wextra
              -Wpedantic
              ${ARM64_TEST_APP_CXX_COMPILE_FLAGS}
              ${ARM64_TEST_APP_SOURCE}
              -o ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMAND ${CMAKE_COMMAND} -E copy ${ARM64_TEST_APP_WINDOWS_EXE} ${ARM64_TEST_APP_IMAGE}
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
              ${ARM64_TEST_APP_OBJCOPY}
              -I binary
              -O ${output_format}
              -B ${output_architecture}
              ${RINGOS_TEST_APP_BINARY_STEM}
              ${RINGOS_TEST_APP_BINARY_STEM}.o
      DEPENDS
        ${ARM64_TEST_APP_COMPILE_CONFIG}
        ${ARM64_TEST_APP_LINK_CONFIG}
        ${ARM64_TEST_APP_SOURCE}
      BYPRODUCTS
        ${ARM64_TEST_APP_IMAGE}
        ${ARM64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded arm64 PE64 C++ test app against the extracted ringos toolchain bundle"
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
      COMMAND ${ARM64_TEST_APP_LLD_LINK}
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
              ${ARM64_TEST_APP_OBJCOPY}
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
