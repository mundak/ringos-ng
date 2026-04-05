get_filename_component(RINGOS_LLVM_ROOT_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(ringos_get_default_compiler_root out_compiler_root)
  if(DEFINED ENV{RINGOS_TOOLCHAIN_ROOT} AND NOT "$ENV{RINGOS_TOOLCHAIN_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{RINGOS_TOOLCHAIN_ROOT}" compiler_root)
  else()
    set(compiler_root ${RINGOS_LLVM_ROOT_REPO_ROOT}/build/toolchain)
  endif()

  set(${out_compiler_root} ${compiler_root} PARENT_SCOPE)
endfunction()

function(ringos_resolve_llvm_root out_compiler_root)
  if(DEFINED RINGOS_ACTIVE_LLVM_ROOT AND NOT RINGOS_ACTIVE_LLVM_ROOT STREQUAL "" AND EXISTS ${RINGOS_ACTIVE_LLVM_ROOT}/bin/clang)
    set(compiler_root ${RINGOS_ACTIVE_LLVM_ROOT})
  elseif(DEFINED ENV{RINGOS_ACTIVE_LLVM_ROOT} AND NOT "$ENV{RINGOS_ACTIVE_LLVM_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{RINGOS_ACTIVE_LLVM_ROOT}" env_active_llvm_root)

    if(EXISTS ${env_active_llvm_root}/bin/clang)
      set(compiler_root ${env_active_llvm_root})
    endif()
  elseif(DEFINED RINGOS_TOOLCHAIN_ROOT AND NOT RINGOS_TOOLCHAIN_ROOT STREQUAL "" AND EXISTS ${RINGOS_TOOLCHAIN_ROOT}/bin/clang)
    set(compiler_root ${RINGOS_TOOLCHAIN_ROOT})
  elseif(DEFINED ENV{RINGOS_TOOLCHAIN_ROOT} AND NOT "$ENV{RINGOS_TOOLCHAIN_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{RINGOS_TOOLCHAIN_ROOT}" env_toolchain_root)

    if(EXISTS ${env_toolchain_root}/bin/clang)
      set(compiler_root ${env_toolchain_root})
    endif()
  endif()

  if(NOT DEFINED compiler_root OR NOT EXISTS ${compiler_root}/bin/clang)
    set(compiler_root ${RINGOS_LLVM_ROOT_REPO_ROOT}/build/toolchain-build/bootstrap-llvm/install)
  endif()

  set(${out_compiler_root} ${compiler_root} PARENT_SCOPE)
endfunction()

function(ringos_find_llvm_tool tool_name out_tool_path)
  ringos_resolve_llvm_root(compiler_root)
  find_program(tool_path NAMES ${ARGN} HINTS ${compiler_root}/bin NO_DEFAULT_PATH REQUIRED)
  set(${out_tool_path} ${tool_path} PARENT_SCOPE)
endfunction()
