include_guard(GLOBAL)

function(ringos_get_sdk_target_triple target_arch out_target_triple)
  if(target_arch STREQUAL "x64")
    set(target_triple x86_64-pc-windows-msvc)
  elseif(target_arch STREQUAL "arm64")
    set(target_triple aarch64-pc-windows-msvc)
  else()
    message(FATAL_ERROR "Unsupported ringos SDK target architecture: ${target_arch}")
  endif()

  set(${out_target_triple} ${target_triple} PARENT_SCOPE)
endfunction()

function(ringos_append_sdk_link_flags target_arch out_link_flags)
  set(link_flags
    -fuse-ld=lld
    -nostdlib
    -Wl,/entry:user_start
    -Wl,/subsystem:console
    -Wl,/nodefaultlib
    -Wl,/filealign:4096)

  if(target_arch STREQUAL "x64")
    list(APPEND link_flags
      -Wl,/fixed
      -Wl,/dynamicbase:no
      -Wl,/base:0x400000)
  elseif(target_arch STREQUAL "arm64")
    list(APPEND link_flags
      -Wl,/fixed:no
      -Wl,/base:0x400000)
  else()
    message(FATAL_ERROR "Unsupported ringos SDK target architecture: ${target_arch}")
  endif()

  set(${out_link_flags} ${link_flags} PARENT_SCOPE)
endfunction()

function(ringos_add_sdk_sysroot target_arch out_target out_target_triple out_sysroot_dir out_include_dir out_lib_dir out_library out_compile_config out_link_config)
  ringos_get_sdk_target_triple(${target_arch} target_triple)

  set(target_name ringos_${target_arch}_sdk_sysroot)
  set(staging_root ${CMAKE_BINARY_DIR}/sdk/${target_arch})
  set(generated_root ${CMAKE_BINARY_DIR}/generated/sdk/${target_arch})
  set(sysroot_dir ${CMAKE_BINARY_DIR}/sysroot/${target_triple})
  set(include_dir ${sysroot_dir}/include)
  set(lib_dir ${sysroot_dir}/lib)
  set(share_dir ${sysroot_dir}/share/ringos)
  set(sysroot_library ${lib_dir}/ringos_sdk.lib)
  set(sysroot_libc ${lib_dir}/ringos_c.lib)
  set(sysroot_compiler_rt ${lib_dir}/clang_rt.builtins.lib)
  set(sysroot_crt0 ${lib_dir}/crt0.obj)
  set(compile_config ${share_dir}/bootstrap-compile.cfg)
  set(link_config ${share_dir}/bootstrap-link.cfg)
  set(toolchain_file ${share_dir}/bootstrap-toolchain.cmake)
  set(target_triple_file ${share_dir}/target-triple.txt)
  set(runtime_manifest ${share_dir}/runtime-manifest.txt)
  set(stamp_file ${share_dir}/sysroot.stamp)

  if(NOT TARGET ${target_name})
    find_program(RINGOS_LLVM_LIB NAMES llvm-lib llvm-lib-18 llvm-lib-17)

    if(NOT RINGOS_LLVM_LIB)
      find_program(RINGOS_LLVM_AR NAMES llvm-ar llvm-ar-18 llvm-ar-17 REQUIRED)
    endif()

    set(sdk_include_dir ${CMAKE_SOURCE_DIR}/user/sdk/include)
    set(sdk_headers
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/console.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/debug.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/handle.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/process.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/rpc.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/sdk.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/status.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/syscalls.h
      ${CMAKE_SOURCE_DIR}/user/sdk/include/ringos/types.h)
    set(libc_include_dir ${CMAKE_SOURCE_DIR}/user/libc/include)
    set(libc_headers
      ${CMAKE_SOURCE_DIR}/user/libc/include/errno.h
      ${CMAKE_SOURCE_DIR}/user/libc/include/stdio.h
      ${CMAKE_SOURCE_DIR}/user/libc/include/stdlib.h
      ${CMAKE_SOURCE_DIR}/user/libc/include/string.h)

    set(rpc_source ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_rpc.c)
    set(console_source ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_console.c)
    set(debug_source ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_debug.c)
    set(process_source ${CMAKE_SOURCE_DIR}/user/sdk/src/ringos_process.c)
    set(crt0_source ${CMAKE_SOURCE_DIR}/user/crt/src/crt0.c)
    set(libc_errno_source ${CMAKE_SOURCE_DIR}/user/libc/src/errno.cpp)
    set(libc_puts_source ${CMAKE_SOURCE_DIR}/user/libc/src/puts.cpp)
    set(libc_stdio_source ${CMAKE_SOURCE_DIR}/user/libc/src/stdio.cpp)
    set(libc_exit_source ${CMAKE_SOURCE_DIR}/user/libc/src/exit.cpp)
    set(libc_stdlib_source ${CMAKE_SOURCE_DIR}/user/libc/src/stdlib.cpp)
    set(libc_string_source ${CMAKE_SOURCE_DIR}/user/libc/src/string.cpp)
    set(compiler_rt_source ${CMAKE_SOURCE_DIR}/user/compiler_rt/src/builtins.c)

    if(target_arch STREQUAL "x64")
      set(syscall_source ${CMAKE_SOURCE_DIR}/user/sdk/x64/ringos_syscall.S)
    elseif(target_arch STREQUAL "arm64")
      set(syscall_source ${CMAKE_SOURCE_DIR}/user/sdk/arm64/ringos_syscall.S)
    else()
      message(FATAL_ERROR "Unsupported ringos SDK target architecture: ${target_arch}")
    endif()

    set(sdk_early_compile_flags -mgeneral-regs-only)

    set(syscall_object ${staging_root}/ringos_syscall.obj)
    set(rpc_object ${staging_root}/ringos_rpc.obj)
    set(console_object ${staging_root}/ringos_console.obj)
    set(debug_object ${staging_root}/ringos_debug.obj)
    set(process_object ${staging_root}/ringos_process.obj)
    set(crt0_object ${staging_root}/crt0.obj)
    set(libc_errno_object ${staging_root}/libc_errno.obj)
    set(libc_puts_object ${staging_root}/libc_puts.obj)
    set(libc_stdio_object ${staging_root}/libc_stdio.obj)
    set(libc_exit_object ${staging_root}/libc_exit.obj)
    set(libc_stdlib_object ${staging_root}/libc_stdlib.obj)
    set(libc_string_object ${staging_root}/libc_string.obj)
    set(compiler_rt_object ${staging_root}/compiler_rt_builtins.obj)

    set(staging_library ${staging_root}/ringos_${target_arch}_sdk.lib)
    set(staging_libc ${staging_root}/ringos_${target_arch}_c.lib)
    set(staging_compiler_rt ${staging_root}/clang_rt.builtins_${target_arch}.lib)

    file(MAKE_DIRECTORY ${generated_root})

    file(TO_CMAKE_PATH "${include_dir}" include_dir_for_config)
    file(TO_CMAKE_PATH "${sysroot_crt0}" crt0_for_config)
    file(TO_CMAKE_PATH "${sysroot_libc}" libc_for_config)
    file(TO_CMAKE_PATH "${sysroot_library}" sdk_library_for_config)
    file(TO_CMAKE_PATH "${sysroot_compiler_rt}" compiler_rt_for_config)

    set(compile_config_source ${generated_root}/bootstrap-compile.cfg)
    set(link_config_source ${generated_root}/bootstrap-link.cfg)
    set(toolchain_file_source ${generated_root}/bootstrap-toolchain.cmake)
    set(target_triple_file_source ${generated_root}/target-triple.txt)
    set(runtime_manifest_source ${generated_root}/runtime-manifest.txt)

    set(compile_config_lines
      --target=${target_triple}
      -fno-stack-protector
      -I
      ${include_dir_for_config})
    string(JOIN "\n" compile_config_contents ${compile_config_lines})
    string(APPEND compile_config_contents "\n")

    ringos_append_sdk_link_flags(${target_arch} link_config_lines)
    list(APPEND link_config_lines
      ${crt0_for_config}
      ${libc_for_config}
      ${sdk_library_for_config}
      ${compiler_rt_for_config})
    string(JOIN "\n" link_config_contents ${link_config_lines})
    string(APPEND link_config_contents "\n")

    string(CONCAT toolchain_contents
      "get_filename_component(RINGOS_SHARE_DIR \"\${CMAKE_CURRENT_LIST_DIR}\" ABSOLUTE)\n"
      "get_filename_component(RINGOS_SYSROOT_DIR \"\${RINGOS_SHARE_DIR}/../..\" ABSOLUTE)\n"
      "set(RINGOS_TARGET_TRIPLE \"${target_triple}\")\n"
      "set(RINGOS_SYSROOT_INCLUDE_DIR \"\${RINGOS_SYSROOT_DIR}/include\")\n"
      "set(RINGOS_SYSROOT_LIB_DIR \"\${RINGOS_SYSROOT_DIR}/lib\")\n"
      "set(RINGOS_SYSROOT_CRT0_OBJECT \"\${RINGOS_SYSROOT_LIB_DIR}/crt0.obj\")\n"
      "set(RINGOS_SYSROOT_SDK_LIBRARY \"\${RINGOS_SYSROOT_LIB_DIR}/ringos_sdk.lib\")\n"
      "set(RINGOS_SYSROOT_LIBC_LIBRARY \"\${RINGOS_SYSROOT_LIB_DIR}/ringos_c.lib\")\n"
      "set(RINGOS_SYSROOT_COMPILER_RT_LIBRARY \"\${RINGOS_SYSROOT_LIB_DIR}/clang_rt.builtins.lib\")\n"
      "set(RINGOS_CLANG_COMPILE_CONFIG \"\${RINGOS_SHARE_DIR}/bootstrap-compile.cfg\")\n"
      "set(RINGOS_CLANG_LINK_CONFIG \"\${RINGOS_SHARE_DIR}/bootstrap-link.cfg\")\n")

    string(CONCAT runtime_manifest_contents
      "target_triple=${target_triple}\n"
      "crt0=lib/crt0.obj\n"
      "sdk=lib/ringos_sdk.lib\n"
      "libc=lib/ringos_c.lib\n"
      "compiler_rt=lib/clang_rt.builtins.lib\n")

    file(WRITE ${compile_config_source} ${compile_config_contents})
    file(WRITE ${link_config_source} ${link_config_contents})
    file(WRITE ${toolchain_file_source} ${toolchain_contents})
    file(WRITE ${target_triple_file_source} "${target_triple}\n")
    file(WRITE ${runtime_manifest_source} ${runtime_manifest_contents})

    if(RINGOS_LLVM_LIB)
      set(sdk_archive_command
        COMMAND ${RINGOS_LLVM_LIB}
                /out:${staging_library}
                ${rpc_object}
                ${console_object}
                ${syscall_object}
                ${debug_object}
                ${process_object})
      set(libc_archive_command
        COMMAND ${RINGOS_LLVM_LIB}
                /out:${staging_libc}
                ${libc_errno_object}
                ${libc_puts_object}
                ${libc_stdio_object}
                ${libc_exit_object}
                ${libc_stdlib_object}
                ${libc_string_object})
      set(compiler_rt_archive_command
        COMMAND ${RINGOS_LLVM_LIB}
                /out:${staging_compiler_rt}
                ${compiler_rt_object})
    else()
      set(sdk_archive_command
        COMMAND ${RINGOS_LLVM_AR}
                rcs
                ${staging_library}
                ${rpc_object}
                ${console_object}
                ${syscall_object}
                ${debug_object}
                ${process_object})
      set(libc_archive_command
        COMMAND ${RINGOS_LLVM_AR}
                rcs
                ${staging_libc}
                ${libc_errno_object}
                ${libc_puts_object}
                ${libc_stdio_object}
                ${libc_exit_object}
                ${libc_stdlib_object}
                ${libc_string_object})
      set(compiler_rt_archive_command
        COMMAND ${RINGOS_LLVM_AR}
                rcs
                ${staging_compiler_rt}
                ${compiler_rt_object})
    endif()

        add_custom_command(
          OUTPUT ${stamp_file}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${staging_root}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${include_dir}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${lib_dir}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${share_dir}
          COMMAND ${CMAKE_ASM_COMPILER}
            --target=${target_triple}
            -ffreestanding
            -fno-stack-protector
            -c ${syscall_source}
            -o ${syscall_object}
          COMMAND ${CMAKE_C_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            ${sdk_early_compile_flags}
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -c ${rpc_source}
            -o ${rpc_object}
          COMMAND ${CMAKE_C_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            ${sdk_early_compile_flags}
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -c ${console_source}
            -o ${console_object}
          COMMAND ${CMAKE_C_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            ${sdk_early_compile_flags}
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -c ${debug_source}
            -o ${debug_object}
          COMMAND ${CMAKE_C_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            ${sdk_early_compile_flags}
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -c ${process_source}
            -o ${process_object}
          COMMAND ${CMAKE_C_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            ${sdk_early_compile_flags}
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${crt0_source}
            -o ${crt0_object}
          COMMAND ${CMAKE_CXX_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${libc_errno_source}
            -o ${libc_errno_object}
          COMMAND ${CMAKE_CXX_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${libc_puts_source}
            -o ${libc_puts_object}
          COMMAND ${CMAKE_CXX_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${libc_stdio_source}
            -o ${libc_stdio_object}
          COMMAND ${CMAKE_CXX_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${libc_exit_source}
            -o ${libc_exit_object}
          COMMAND ${CMAKE_CXX_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${libc_stdlib_source}
            -o ${libc_stdlib_object}
          COMMAND ${CMAKE_CXX_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -I ${sdk_include_dir}
            -I ${libc_include_dir}
            -c ${libc_string_source}
            -o ${libc_string_object}
          COMMAND ${CMAKE_C_COMPILER}
            --target=${target_triple}
            -O2
            -ffreestanding
            -fno-stack-protector
            -fno-builtin
            -Wall
            -Wextra
            -Wpedantic
            -c ${compiler_rt_source}
            -o ${compiler_rt_object}
          ${sdk_archive_command}
          ${libc_archive_command}
          ${compiler_rt_archive_command}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${sdk_include_dir} ${include_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${libc_include_dir} ${include_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${crt0_object} ${sysroot_crt0}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${staging_library} ${sysroot_library}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${staging_libc} ${sysroot_libc}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${staging_compiler_rt} ${sysroot_compiler_rt}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${compile_config_source} ${compile_config}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${link_config_source} ${link_config}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${toolchain_file_source} ${toolchain_file}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${target_triple_file_source} ${target_triple_file}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${runtime_manifest_source} ${runtime_manifest}
      COMMAND ${CMAKE_COMMAND} -E touch ${stamp_file}
      DEPENDS
        ${sdk_headers}
        ${libc_headers}
        ${syscall_source}
        ${rpc_source}
        ${console_source}
        ${debug_source}
        ${process_source}
        ${crt0_source}
        ${libc_errno_source}
        ${libc_stdio_source}
        ${libc_stdlib_source}
        ${libc_string_source}
        ${compiler_rt_source}
        ${compile_config_source}
        ${link_config_source}
        ${toolchain_file_source}
        ${target_triple_file_source}
        ${runtime_manifest_source}
      BYPRODUCTS
        ${syscall_object}
        ${rpc_object}
        ${console_object}
        ${debug_object}
        ${process_object}
        ${crt0_object}
        ${libc_errno_object}
        ${libc_stdio_object}
        ${libc_stdlib_object}
        ${libc_string_object}
        ${compiler_rt_object}
        ${staging_library}
        ${staging_libc}
        ${staging_compiler_rt}
        ${sysroot_crt0}
        ${sysroot_library}
        ${sysroot_libc}
        ${sysroot_compiler_rt}
        ${compile_config}
        ${link_config}
        ${toolchain_file}
        ${target_triple_file}
        ${runtime_manifest}
      COMMENT "Staging ringos SDK sysroot for ${target_triple}"
      VERBATIM
    )

    add_custom_target(${target_name} DEPENDS ${stamp_file})
  endif()

  set(${out_target} ${target_name} PARENT_SCOPE)
  set(${out_target_triple} ${target_triple} PARENT_SCOPE)
  set(${out_sysroot_dir} ${sysroot_dir} PARENT_SCOPE)
  set(${out_include_dir} ${include_dir} PARENT_SCOPE)
  set(${out_lib_dir} ${lib_dir} PARENT_SCOPE)
  set(${out_library} ${sysroot_library} PARENT_SCOPE)
  set(${out_compile_config} ${compile_config} PARENT_SCOPE)
  set(${out_link_config} ${link_config} PARENT_SCOPE)
endfunction()
