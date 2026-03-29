function(
  ringos_add_embedded_arm64_test_app
  dependency_target
  output_format
  output_architecture
  out_object)
  set(options)
  set(oneValueArgs BINARY_STEM SOURCE_PATH)
  cmake_parse_arguments(RINGOS_TEST_APP "${options}" "${oneValueArgs}" "" ${ARGN})

  find_program(RINGOS_LLD_LINK NAMES lld-link lld-link-18 lld-link-17 REQUIRED)
  find_program(RINGOS_LLVM_LIB NAMES llvm-lib llvm-lib-18 llvm-lib-17)

  if(NOT RINGOS_LLVM_LIB)
    find_program(RINGOS_LLVM_AR NAMES llvm-ar llvm-ar-18 llvm-ar-17 REQUIRED)
  endif()

  if(NOT RINGOS_TEST_APP_SOURCE_PATH)
    set(RINGOS_TEST_APP_SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/test_app.c)
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_STEM)
    set(RINGOS_TEST_APP_BINARY_STEM ringos_test_app_arm64_pe64_image)
  endif()

  set(ARM64_TEST_APP_SOURCE ${RINGOS_TEST_APP_SOURCE_PATH})
  set(ARM64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.obj)
  set(ARM64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM})
  set(ARM64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.o)
  set(RINGOS_SDK_SYSCALL_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_ringos_arm64_syscall.obj)
  set(RINGOS_SDK_DEBUG_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_ringos_arm64_debug.obj)
  set(RINGOS_SDK_PROCESS_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_ringos_arm64_process.obj)
  set(RINGOS_USER_ENTRY_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_user_entry.obj)
  set(RINGOS_USER_STDIO_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_user_stdio.obj)
  set(RINGOS_SDK_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}_ringos_arm64_sdk.lib)

  set(RINGOS_SDK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/user/sdk/include)
  set(RINGOS_USER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/user/libc/include)
  set(RINGOS_SDK_HEADERS
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/debug.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/handle.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/process.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/sdk.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/status.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/syscalls.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/types.h
    ${CMAKE_SOURCE_DIR}/user/libc/include/stdio.h)
  set(RINGOS_SDK_SYSCALL_SOURCE ${CMAKE_SOURCE_DIR}/user/sdk/arm64/ringos_syscall.S)
  set(RINGOS_SDK_DEBUG_SOURCE ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_debug.c)
  set(RINGOS_SDK_PROCESS_SOURCE ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_process.c)
  set(RINGOS_USER_ENTRY_SOURCE ${CMAKE_SOURCE_DIR}/user/crt/entry.c)
  set(RINGOS_USER_STDIO_SOURCE ${CMAKE_SOURCE_DIR}/user/libc/stdio.c)
  set(RINGOS_SDK_ARCHIVE_OBJECTS
    ${RINGOS_SDK_SYSCALL_OBJECT}
    ${RINGOS_SDK_DEBUG_OBJECT}
    ${RINGOS_SDK_PROCESS_OBJECT}
    ${RINGOS_USER_ENTRY_OBJECT}
    ${RINGOS_USER_STDIO_OBJECT})

  if(RINGOS_LLVM_LIB)
    set(RINGOS_SDK_ARCHIVE_COMMAND
      COMMAND ${RINGOS_LLVM_LIB}
              /out:${RINGOS_SDK_LIBRARY}
              ${RINGOS_SDK_ARCHIVE_OBJECTS})
  else()
    set(RINGOS_SDK_ARCHIVE_COMMAND
      COMMAND ${RINGOS_LLVM_AR}
              rcs
              ${RINGOS_SDK_LIBRARY}
              ${RINGOS_SDK_ARCHIVE_OBJECTS})
  endif()

  add_custom_command(
    OUTPUT ${ARM64_TEST_APP_IMAGE_OBJECT}
    COMMAND ${CMAKE_ASM_COMPILER}
            --target=aarch64-pc-windows-msvc
            -ffreestanding
            -fno-stack-protector
            -c ${RINGOS_SDK_SYSCALL_SOURCE}
            -o ${RINGOS_SDK_SYSCALL_OBJECT}
    COMMAND ${CMAKE_C_COMPILER}
            --target=aarch64-pc-windows-msvc
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${RINGOS_SDK_INCLUDE_DIR}
            -c ${RINGOS_SDK_DEBUG_SOURCE}
            -o ${RINGOS_SDK_DEBUG_OBJECT}
    COMMAND ${CMAKE_C_COMPILER}
            --target=aarch64-pc-windows-msvc
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${RINGOS_SDK_INCLUDE_DIR}
            -c ${RINGOS_SDK_PROCESS_SOURCE}
            -o ${RINGOS_SDK_PROCESS_OBJECT}
    COMMAND ${CMAKE_C_COMPILER}
            --target=aarch64-pc-windows-msvc
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${RINGOS_SDK_INCLUDE_DIR}
            -I ${RINGOS_USER_INCLUDE_DIR}
            -c ${RINGOS_USER_ENTRY_SOURCE}
            -o ${RINGOS_USER_ENTRY_OBJECT}
    COMMAND ${CMAKE_C_COMPILER}
            --target=aarch64-pc-windows-msvc
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${RINGOS_SDK_INCLUDE_DIR}
            -I ${RINGOS_USER_INCLUDE_DIR}
            -c ${RINGOS_USER_STDIO_SOURCE}
            -o ${RINGOS_USER_STDIO_OBJECT}
    ${RINGOS_SDK_ARCHIVE_COMMAND}
    COMMAND ${CMAKE_C_COMPILER}
            --target=aarch64-pc-windows-msvc
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${RINGOS_SDK_INCLUDE_DIR}
            -I ${RINGOS_USER_INCLUDE_DIR}
            -c ${ARM64_TEST_APP_SOURCE}
            -o ${ARM64_TEST_APP_OBJECT}
    COMMAND ${RINGOS_LLD_LINK}
            /machine:arm64
            /entry:user_start
            /subsystem:console
            /nodefaultlib
            /fixed:no
            /base:0x400000
            /filealign:4096
            /out:${ARM64_TEST_APP_IMAGE}
            ${ARM64_TEST_APP_OBJECT}
            ${RINGOS_SDK_LIBRARY}
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_OBJCOPY}
            -I binary
            -O ${output_format}
            -B ${output_architecture}
            ${RINGOS_TEST_APP_BINARY_STEM}
            ${RINGOS_TEST_APP_BINARY_STEM}.o
    DEPENDS
      ${RINGOS_SDK_HEADERS}
      ${RINGOS_SDK_SYSCALL_SOURCE}
      ${RINGOS_SDK_DEBUG_SOURCE}
      ${RINGOS_SDK_PROCESS_SOURCE}
      ${RINGOS_USER_ENTRY_SOURCE}
      ${RINGOS_USER_STDIO_SOURCE}
      ${ARM64_TEST_APP_SOURCE}
    BYPRODUCTS
      ${RINGOS_SDK_SYSCALL_OBJECT}
      ${RINGOS_SDK_DEBUG_OBJECT}
      ${RINGOS_SDK_PROCESS_OBJECT}
      ${RINGOS_USER_ENTRY_OBJECT}
      ${RINGOS_USER_STDIO_OBJECT}
      ${RINGOS_SDK_LIBRARY}
      ${ARM64_TEST_APP_OBJECT}
      ${ARM64_TEST_APP_IMAGE}
    COMMENT "Building embedded arm64 PE64 test app with the ringos C SDK and user runtime"
    VERBATIM
  )

  add_custom_target(${dependency_target} DEPENDS ${ARM64_TEST_APP_IMAGE_OBJECT})

  set_source_files_properties(${ARM64_TEST_APP_IMAGE_OBJECT}
    PROPERTIES
      EXTERNAL_OBJECT TRUE
      GENERATED TRUE
  )

  set(${out_object} ${ARM64_TEST_APP_IMAGE_OBJECT} PARENT_SCOPE)
endfunction()