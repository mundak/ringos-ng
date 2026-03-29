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
  find_program(RINGOS_LLVM_LIB NAMES llvm-lib llvm-lib-18 llvm-lib-17)

  if(NOT RINGOS_LLVM_LIB)
    find_program(RINGOS_LLVM_AR NAMES llvm-ar llvm-ar-18 llvm-ar-17 REQUIRED)
  endif()

  if(NOT RINGOS_TEST_APP_SOURCE_PATH)
    set(RINGOS_TEST_APP_SOURCE_PATH ${CMAKE_SOURCE_DIR}/user/samples/test_app.c)
  endif()

  if(NOT RINGOS_TEST_APP_BINARY_STEM)
    set(RINGOS_TEST_APP_BINARY_STEM ringos_test_app_x64_pe64_image)
  endif()

  get_filename_component(RINGOS_TEST_APP_SOURCE_EXTENSION ${RINGOS_TEST_APP_SOURCE_PATH} EXT)

  set(X64_TEST_APP_SOURCE ${RINGOS_TEST_APP_SOURCE_PATH})
  set(X64_TEST_APP_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.obj)
  set(X64_TEST_APP_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM})
  set(X64_TEST_APP_IMAGE_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.o)
  set(X64_TEST_APP_WINDOWS_EXE ${CMAKE_CURRENT_BINARY_DIR}/${RINGOS_TEST_APP_BINARY_STEM}.exe)

  set(RINGOS_SDK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/user/sdk/include)
  set(RINGOS_SDK_HEADERS
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/debug.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/handle.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/process.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/sdk.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/status.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/syscalls.h
    ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/types.h)
  set(RINGOS_SDK_SYSCALL_SOURCE ${CMAKE_SOURCE_DIR}/user/sdk/x64/ringos_syscall.S)
  set(RINGOS_SDK_DEBUG_SOURCE ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_debug.c)
  set(RINGOS_SDK_PROCESS_SOURCE ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_process.c)
  set(RINGOS_SDK_SYSCALL_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_syscall.obj)
  set(RINGOS_SDK_DEBUG_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_debug.obj)
  set(RINGOS_SDK_PROCESS_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/ringos_process.obj)
  set(RINGOS_SDK_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/ringos_sdk.lib)
  set(RINGOS_SDK_SYSROOT_DIR ${CMAKE_BINARY_DIR}/sysroot/x86_64-pc-windows-msvc)
  set(RINGOS_SDK_SYSROOT_INCLUDE_DIR ${RINGOS_SDK_SYSROOT_DIR}/include)
  set(RINGOS_SDK_SYSROOT_LIB_DIR ${RINGOS_SDK_SYSROOT_DIR}/lib)
  set(RINGOS_SDK_SYSROOT_LIBRARY ${RINGOS_SDK_SYSROOT_LIB_DIR}/ringos_sdk.lib)

  if(RINGOS_LLVM_LIB)
    set(RINGOS_SDK_ARCHIVE_COMMAND
      COMMAND ${RINGOS_LLVM_LIB}
              /out:${RINGOS_SDK_LIBRARY}
              ${RINGOS_SDK_SYSCALL_OBJECT}
              ${RINGOS_SDK_DEBUG_OBJECT}
              ${RINGOS_SDK_PROCESS_OBJECT})
  else()
    set(RINGOS_SDK_ARCHIVE_COMMAND
      COMMAND ${RINGOS_LLVM_AR}
              rcs
              ${RINGOS_SDK_LIBRARY}
              ${RINGOS_SDK_SYSCALL_OBJECT}
              ${RINGOS_SDK_DEBUG_OBJECT}
              ${RINGOS_SDK_PROCESS_OBJECT})
  endif()

  if(RINGOS_TEST_APP_SOURCE_EXTENSION STREQUAL ".c")
    add_custom_command(
      OUTPUT ${X64_TEST_APP_IMAGE_OBJECT}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RINGOS_SDK_SYSROOT_INCLUDE_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RINGOS_SDK_SYSROOT_LIB_DIR}
      COMMAND ${CMAKE_ASM_COMPILER}
              --target=x86_64-pc-windows-msvc
              -ffreestanding
              -fno-stack-protector
              -c ${RINGOS_SDK_SYSCALL_SOURCE}
              -o ${RINGOS_SDK_SYSCALL_OBJECT}
      COMMAND ${CMAKE_C_COMPILER}
              --target=x86_64-pc-windows-msvc
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
              --target=x86_64-pc-windows-msvc
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
      ${RINGOS_SDK_ARCHIVE_COMMAND}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${RINGOS_SDK_INCLUDE_DIR} ${RINGOS_SDK_SYSROOT_INCLUDE_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_SDK_LIBRARY} ${RINGOS_SDK_SYSROOT_LIBRARY}
      COMMAND ${CMAKE_C_COMPILER}
              --target=x86_64-pc-windows-msvc
              -O2
              -ffreestanding
              -fno-stack-protector
              -fno-builtin
              -Wall
              -Wextra
              -Wpedantic
              -I ${RINGOS_SDK_SYSROOT_INCLUDE_DIR}
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
              /libpath:${RINGOS_SDK_SYSROOT_LIB_DIR}
              ${X64_TEST_APP_OBJECT}
              ringos_sdk.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${X64_TEST_APP_IMAGE} ${X64_TEST_APP_WINDOWS_EXE}
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
        ${X64_TEST_APP_SOURCE}
      BYPRODUCTS
        ${RINGOS_SDK_SYSCALL_OBJECT}
        ${RINGOS_SDK_DEBUG_OBJECT}
        ${RINGOS_SDK_PROCESS_OBJECT}
        ${RINGOS_SDK_LIBRARY}
        ${RINGOS_SDK_SYSROOT_LIBRARY}
        ${X64_TEST_APP_OBJECT}
        ${X64_TEST_APP_IMAGE}
        ${X64_TEST_APP_WINDOWS_EXE}
      COMMENT "Building embedded x64 PE64 test app with the ringos C SDK"
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
