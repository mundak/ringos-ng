include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/ringos_sdk_sysroot.cmake)

get_filename_component(RINGOS_REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

function(ringos_get_native_target_triple target_arch out_target_triple)
  if(target_arch STREQUAL "x64")
    set(target_triple x86_64-unknown-ringos-msvc)
  elseif(target_arch STREQUAL "arm64")
    set(target_triple aarch64-unknown-ringos-msvc)
  else()
    message(FATAL_ERROR "Unsupported ringos native target architecture: ${target_arch}")
  endif()

  set(${out_target_triple} ${target_triple} PARENT_SCOPE)
endfunction()

function(ringos_get_default_toolchain_install_root out_install_root)
  if(WIN32 AND DEFINED ENV{LOCALAPPDATA})
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/ringos/toolchain" install_root)
  elseif(DEFINED ENV{HOME})
    file(TO_CMAKE_PATH "$ENV{HOME}/.cache/ringos/toolchain" install_root)
  else()
    set(install_root ${RINGOS_REPO_ROOT}/build/installed-toolchain)
  endif()

  set(${out_install_root} ${install_root} PARENT_SCOPE)
endfunction()

function(ringos_get_default_previous_stage_toolchain_root out_toolchain_root)
  if(WIN32 AND DEFINED ENV{LOCALAPPDATA})
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/ringos/native-llvm-toolchain" toolchain_root)
  elseif(DEFINED ENV{HOME})
    file(TO_CMAKE_PATH "$ENV{HOME}/.cache/ringos/native-llvm-toolchain" toolchain_root)
  else()
    set(toolchain_root ${RINGOS_REPO_ROOT}/build/native-llvm-toolchain)
  endif()

  set(${out_toolchain_root} ${toolchain_root} PARENT_SCOPE)
endfunction()

function(ringos_get_llvm_ref out_llvm_ref)
  if(DEFINED RINGOS_LLVM_REF AND NOT RINGOS_LLVM_REF STREQUAL "")
    set(llvm_ref ${RINGOS_LLVM_REF})
  elseif(DEFINED ENV{RINGOS_LLVM_REF} AND NOT "$ENV{RINGOS_LLVM_REF}" STREQUAL "")
    set(llvm_ref $ENV{RINGOS_LLVM_REF})
  else()
    set(llvm_ref llvmorg-18.1.8)
  endif()

  set(${out_llvm_ref} ${llvm_ref} PARENT_SCOPE)
endfunction()

function(ringos_get_expected_clang_resource_version out_resource_version)
  unset(resource_version)

  if(DEFINED RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT AND NOT RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT STREQUAL "")
    find_program(ringos_previous_stage_clang
      NAMES clang clang-18 clang-17
      HINTS ${RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT}/bin
      NO_DEFAULT_PATH)

    if(ringos_previous_stage_clang)
      execute_process(
        COMMAND ${ringos_previous_stage_clang} --print-resource-dir
        RESULT_VARIABLE ringos_previous_stage_clang_result
        OUTPUT_VARIABLE ringos_previous_stage_clang_resource_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE)

      if(ringos_previous_stage_clang_result EQUAL 0 AND EXISTS ${ringos_previous_stage_clang_resource_dir})
        get_filename_component(resource_version ${ringos_previous_stage_clang_resource_dir} NAME)
      endif()
    endif()
  endif()

  if(NOT DEFINED resource_version OR resource_version STREQUAL "")
    ringos_get_llvm_ref(llvm_ref)

    if(llvm_ref MATCHES "^llvmorg-(.+)$")
      set(resource_version ${CMAKE_MATCH_1})
    else()
      set(resource_version ${llvm_ref})
    endif()
  endif()

  set(${out_resource_version} ${resource_version} PARENT_SCOPE)
endfunction()

function(ringos_collect_custom_llvm_input_files out_input_files)
  file(GLOB llvm_patch_files LIST_DIRECTORIES FALSE ${RINGOS_REPO_ROOT}/tools/llvm/patches/*.patch)
  list(SORT llvm_patch_files)

  set(input_files
    ${RINGOS_REPO_ROOT}/tools/llvm/build-clang-toolchain.sh
    ${RINGOS_REPO_ROOT}/tools/llvm/ensure-libcxx-source.sh
    ${llvm_patch_files})

  set(${out_input_files} ${input_files} PARENT_SCOPE)
endfunction()

function(ringos_collect_installed_toolchain_input_files target_arch out_input_files)
  ringos_collect_libcxx_headers(libcxx_headers)
  ringos_collect_custom_llvm_input_files(custom_llvm_input_files)

  set(input_files
    ${RINGOS_REPO_ROOT}/cmake/ringos_sdk_sysroot.cmake
    ${RINGOS_REPO_ROOT}/cmake/ringos_installed_toolchain.cmake
    ${RINGOS_REPO_ROOT}/cmake/ringos_toolchain_identity.cmake
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/console.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/debug.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/handle.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/process.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/rpc.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/sdk.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/status.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/syscalls.h
    ${RINGOS_REPO_ROOT}/user/sdk/include/ringos/types.h
    ${RINGOS_REPO_ROOT}/user/libc/include/errno.h
    ${RINGOS_REPO_ROOT}/user/libc/include/stdio.h
    ${RINGOS_REPO_ROOT}/user/libc/include/stdlib.h
    ${RINGOS_REPO_ROOT}/user/libc/include/string.h
    ${RINGOS_REPO_ROOT}/user/libcxx/__assertion_handler
    ${RINGOS_REPO_ROOT}/user/libcxx/__config_site
    ${RINGOS_REPO_ROOT}/user/sdk/src/ringos_rpc.c
    ${RINGOS_REPO_ROOT}/user/sdk/src/ringos_console.c
    ${RINGOS_REPO_ROOT}/user/sdk/src/ringos_debug.c
    ${RINGOS_REPO_ROOT}/user/sdk/src/ringos_process.c
    ${RINGOS_REPO_ROOT}/user/crt/src/crt0.c
    ${RINGOS_REPO_ROOT}/user/libc/src/errno.cpp
    ${RINGOS_REPO_ROOT}/user/libc/src/puts.cpp
    ${RINGOS_REPO_ROOT}/user/libc/src/stdio.cpp
    ${RINGOS_REPO_ROOT}/user/libc/src/exit.cpp
    ${RINGOS_REPO_ROOT}/user/libc/src/stdlib.cpp
    ${RINGOS_REPO_ROOT}/user/libc/src/string.cpp
    ${RINGOS_REPO_ROOT}/user/compiler_rt/src/builtins.c
    ${custom_llvm_input_files})

  if(libcxx_headers)
    list(APPEND input_files ${libcxx_headers})
  endif()

  if(target_arch STREQUAL "x64")
    list(APPEND input_files ${RINGOS_REPO_ROOT}/user/sdk/x64/ringos_syscall.S)
  elseif(target_arch STREQUAL "arm64")
    list(APPEND input_files ${RINGOS_REPO_ROOT}/user/sdk/arm64/ringos_syscall.S)
  else()
    message(FATAL_ERROR "Unsupported ringos native target architecture: ${target_arch}")
  endif()

  set(${out_input_files} ${input_files} PARENT_SCOPE)
endfunction()

function(ringos_get_toolchain_hash_input_label input_file out_label)
  file(RELATIVE_PATH input_file_label ${RINGOS_REPO_ROOT} ${input_file})

  string(FIND "${input_file_label}" "../" input_file_label_parent_prefix)

  if(input_file_label_parent_prefix EQUAL 0)
    message(FATAL_ERROR "Toolchain hash input is outside the source tree: ${input_file}")
  endif()

  set(${out_label} ${input_file_label} PARENT_SCOPE)
endfunction()

function(ringos_compute_installed_toolchain_id target_arch out_toolchain_id_prefix out_toolchain_id)
  ringos_get_native_target_triple(${target_arch} native_target_triple)
  ringos_get_llvm_ref(llvm_ref)
  ringos_get_expected_clang_resource_version(clang_resource_version)
  ringos_collect_installed_toolchain_input_files(${target_arch} toolchain_input_files)

  set(toolchain_hash_material
    "target_arch=${target_arch}\n"
    "native_target_triple=${native_target_triple}\n"
    "driver_target_triple=${native_target_triple}\n"
    "toolchain_mode=ringos-native\n"
    "llvm_ref=${llvm_ref}\n"
    "clang_resource_version=${clang_resource_version}\n")

  foreach(input_file IN LISTS toolchain_input_files)
    file(SHA256 ${input_file} current_hash)
    ringos_get_toolchain_hash_input_label(${input_file} input_file_label)
    string(APPEND toolchain_hash_material "${input_file_label}=${current_hash}\n")
  endforeach()

  string(SHA256 toolchain_id "${toolchain_hash_material}")
  string(SUBSTRING ${toolchain_id} 0 20 toolchain_id_prefix)

  set(${out_toolchain_id_prefix} ${toolchain_id_prefix} PARENT_SCOPE)
  set(${out_toolchain_id} ${toolchain_id} PARENT_SCOPE)
endfunction()

function(ringos_compute_bundle_id x64_toolchain_id arm64_toolchain_id out_bundle_id)
  string(SHA256 bundle_hash "x64=${x64_toolchain_id}\narm64=${arm64_toolchain_id}\n")
  string(SUBSTRING ${bundle_hash} 0 20 bundle_id)
  set(${out_bundle_id} ${bundle_id} PARENT_SCOPE)
endfunction()
