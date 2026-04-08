function(ringos_get_bundled_toolchain_root out_root)
  if(DEFINED RINGOS_TOOLCHAIN_ROOT AND NOT RINGOS_TOOLCHAIN_ROOT STREQUAL "")
    set(toolchain_root "${RINGOS_TOOLCHAIN_ROOT}")
  elseif(DEFINED ENV{RINGOS_TOOLCHAIN_ROOT} AND NOT "$ENV{RINGOS_TOOLCHAIN_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{RINGOS_TOOLCHAIN_ROOT}" toolchain_root)
  else()
    set(toolchain_root "${CMAKE_SOURCE_DIR}/build/toolchain")
  endif()

  set(${out_root} "${toolchain_root}" PARENT_SCOPE)
endfunction()

function(ringos_resolve_bundled_test_app_tools target_arch prefix)
  ringos_get_bundled_toolchain_root(toolchain_root)

  if(target_arch STREQUAL "x64")
    set(target_triple x86_64-unknown-ringos-msvc)
  elseif(target_arch STREQUAL "arm64")
    set(target_triple aarch64-unknown-ringos-msvc)
  else()
    message(FATAL_ERROR "Unsupported test app target architecture: ${target_arch}")
  endif()

  set(toolchain_file "${toolchain_root}/cmake/ringos-toolchain.cmake")
  set(compile_config "${toolchain_root}/share/ringos/compile-${target_arch}.cfg")
  set(link_config "${toolchain_root}/share/ringos/link-${target_arch}.cfg")
  set(cxx_include_dir "${toolchain_root}/sysroots/${target_triple}/include/c++/v1")
  set(cxx_compile_flags
    -std=c++20
    -fno-ms-compatibility
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -nostdinc++
    -isystem
    "${cxx_include_dir}")

  foreach(required_path
      "${toolchain_file}"
      "${compile_config}"
      "${link_config}")
    if(NOT EXISTS "${required_path}")
      message(FATAL_ERROR
        "Bundled toolchain input is missing: ${required_path}. "
        "Extract build/toolchain before building embedded test app images.")
    endif()
  endforeach()

  find_program(test_app_clang
    NAMES clang clang-18 clang-17
    HINTS "${toolchain_root}/bin"
    NO_DEFAULT_PATH
    REQUIRED)
  find_program(test_app_clangxx
    NAMES clang++ clang++-18 clang++-17
    HINTS "${toolchain_root}/bin"
    NO_DEFAULT_PATH
    REQUIRED)
  find_program(test_app_lld_link
    NAMES lld-link lld-link-18 lld-link-17
    HINTS "${toolchain_root}/bin"
    NO_DEFAULT_PATH
    REQUIRED)
  find_program(test_app_objcopy
    NAMES llvm-objcopy llvm-objcopy-18 llvm-objcopy-17
    HINTS "${toolchain_root}/bin"
    NO_DEFAULT_PATH
    REQUIRED)

  set(${prefix}_TOOLCHAIN_ROOT "${toolchain_root}" PARENT_SCOPE)
  set(${prefix}_TOOLCHAIN_FILE "${toolchain_file}" PARENT_SCOPE)
  set(${prefix}_COMPILE_CONFIG "${compile_config}" PARENT_SCOPE)
  set(${prefix}_LINK_CONFIG "${link_config}" PARENT_SCOPE)
  set(${prefix}_CXX_INCLUDE_DIR "${cxx_include_dir}" PARENT_SCOPE)
  set(${prefix}_CXX_COMPILE_FLAGS ${cxx_compile_flags} PARENT_SCOPE)
  set(${prefix}_TARGET_TRIPLE "${target_triple}" PARENT_SCOPE)
  set(${prefix}_CLANG "${test_app_clang}" PARENT_SCOPE)
  set(${prefix}_CLANGXX "${test_app_clangxx}" PARENT_SCOPE)
  set(${prefix}_LLD_LINK "${test_app_lld_link}" PARENT_SCOPE)
  set(${prefix}_OBJCOPY "${test_app_objcopy}" PARENT_SCOPE)
endfunction()
