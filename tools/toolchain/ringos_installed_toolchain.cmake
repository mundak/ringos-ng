include_guard(GLOBAL)

include(${CMAKE_SOURCE_DIR}/tools/toolchain/ringos_toolchain_common.cmake)

function(ringos_get_toolchain_version out_version)
  if(DEFINED RINGOS_TOOLCHAIN_VERSION AND NOT RINGOS_TOOLCHAIN_VERSION STREQUAL "")
    set(toolchain_version ${RINGOS_TOOLCHAIN_VERSION})
  elseif(DEFINED ENV{RINGOS_TOOLCHAIN_VERSION} AND NOT "$ENV{RINGOS_TOOLCHAIN_VERSION}" STREQUAL "")
    set(toolchain_version $ENV{RINGOS_TOOLCHAIN_VERSION})
  else()
    set(toolchain_version dev-local)
  endif()

  set(${out_version} ${toolchain_version} PARENT_SCOPE)
endfunction()

function(ringos_resolve_tool_program tool_name out_path)
  ringos_get_active_llvm_root(active_llvm_root)

  if(NOT EXISTS ${active_llvm_root}/bin/clang)
    message(FATAL_ERROR
      "Active LLVM root does not contain bin/clang: ${active_llvm_root}")
  endif()

  string(MAKE_C_IDENTIFIER "${tool_name}" tool_path_suffix)
  set(tool_path_var "ringos_resolved_tool_path_${tool_path_suffix}")
  unset(${tool_path_var})
  unset(${tool_path_var} CACHE)

  find_program(${tool_path_var} NAMES ${ARGN} HINTS ${active_llvm_root}/bin NO_DEFAULT_PATH)
  set(tool_path ${${tool_path_var}})

  if(NOT tool_path)
    message(FATAL_ERROR "Unable to find required tool '${tool_name}'.")
  endif()

  set(${out_path} ${tool_path} PARENT_SCOPE)
endfunction()

function(ringos_resolve_clang_resource_dir clang_path out_resource_dir out_resource_version)
  execute_process(
    COMMAND ${clang_path} --print-resource-dir
    RESULT_VARIABLE clang_resource_dir_result
    OUTPUT_VARIABLE clang_resource_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT clang_resource_dir_result EQUAL 0 OR NOT EXISTS ${clang_resource_dir})
    message(FATAL_ERROR "Unable to resolve clang resource directory from ${clang_path}")
  endif()

  get_filename_component(clang_resource_version ${clang_resource_dir} NAME)

  set(${out_resource_dir} ${clang_resource_dir} PARENT_SCOPE)
  set(${out_resource_version} ${clang_resource_version} PARENT_SCOPE)
endfunction()

function(ringos_generate_installed_toolchain_bundle target_arch out_target out_bundle_root out_toolchain_file)
  ringos_get_active_llvm_root(active_llvm_root)
  ringos_get_native_target_triple(${target_arch} native_target_triple)

  ringos_add_sdk_sysroot(
    ${target_arch}
    RINGOS_SDK_SYSROOT_TARGET
    RINGOS_SDK_TARGET_TRIPLE
    RINGOS_SDK_SYSROOT_DIR
    RINGOS_SDK_SYSROOT_INCLUDE_DIR
    RINGOS_SDK_SYSROOT_LIB_DIR
    RINGOS_SDK_SYSROOT_LIBRARY
    RINGOS_SDK_COMPILE_CONFIG
    RINGOS_SDK_LINK_CONFIG)

  ringos_resolve_tool_program(clang RINGOS_TOOLCHAIN_CLANG clang clang-18 clang-17)
  ringos_resolve_tool_program(clang++ RINGOS_TOOLCHAIN_CLANGXX clang++ clang++-18 clang++-17)
  ringos_resolve_tool_program(lld-link RINGOS_TOOLCHAIN_LLD_LINK lld-link lld-link-18 lld-link-17)
  ringos_resolve_tool_program(llvm-ar RINGOS_TOOLCHAIN_LLVM_AR llvm-ar llvm-ar-18 llvm-ar-17)
  ringos_resolve_tool_program(llvm-ranlib RINGOS_TOOLCHAIN_LLVM_RANLIB llvm-ranlib llvm-ranlib-18 llvm-ranlib-17)
  ringos_resolve_tool_program(llvm-objcopy RINGOS_TOOLCHAIN_LLVM_OBJCOPY llvm-objcopy llvm-objcopy-18 llvm-objcopy-17)

  find_program(RINGOS_TOOLCHAIN_LD_LLD NAMES ld.lld ld.lld-18 ld.lld-17 HINTS ${active_llvm_root}/bin NO_DEFAULT_PATH)
  find_program(RINGOS_TOOLCHAIN_LLVM_LIB NAMES llvm-lib llvm-lib-18 llvm-lib-17 HINTS ${active_llvm_root}/bin NO_DEFAULT_PATH)

  get_filename_component(toolchain_clang_name ${RINGOS_TOOLCHAIN_CLANG} NAME)
  get_filename_component(toolchain_clangxx_name ${RINGOS_TOOLCHAIN_CLANGXX} NAME)
  get_filename_component(toolchain_lld_link_name ${RINGOS_TOOLCHAIN_LLD_LINK} NAME)
  get_filename_component(toolchain_llvm_ar_name ${RINGOS_TOOLCHAIN_LLVM_AR} NAME)
  get_filename_component(toolchain_llvm_ranlib_name ${RINGOS_TOOLCHAIN_LLVM_RANLIB} NAME)
  get_filename_component(toolchain_llvm_objcopy_name ${RINGOS_TOOLCHAIN_LLVM_OBJCOPY} NAME)

  if(RINGOS_TOOLCHAIN_LD_LLD)
    get_filename_component(toolchain_ld_lld_name ${RINGOS_TOOLCHAIN_LD_LLD} NAME)
  endif()

  if(RINGOS_TOOLCHAIN_LLVM_LIB)
    get_filename_component(toolchain_llvm_lib_name ${RINGOS_TOOLCHAIN_LLVM_LIB} NAME)
  endif()

  ringos_get_toolchain_version(toolchain_version)
  ringos_get_llvm_ref(llvm_ref)
  ringos_resolve_clang_resource_dir(${RINGOS_TOOLCHAIN_CLANG} clang_resource_dir clang_resource_version)

  ringos_collect_installed_toolchain_input_files(${target_arch} toolchain_input_files)

  set(target_name ringos_${target_arch}_installed_toolchain)
  set(bundle_root ${RINGOS_TOOLCHAIN_ROOT})
  set(bundle_bin_dir ${bundle_root}/bin)
  set(bundle_lib_dir ${bundle_root}/lib)
  set(bundle_resource_dir ${bundle_lib_dir}/clang/${clang_resource_version})
  set(bundle_sysroot_dir ${bundle_root}/sysroots/${native_target_triple})
  set(bundle_share_dir ${bundle_root}/share/ringos)
  set(bundle_cmake_dir ${bundle_root}/cmake)
  set(bundle_toolchain_file ${bundle_cmake_dir}/ringos-${target_arch}-toolchain.cmake)
  set(bundle_manifest_file ${bundle_share_dir}/toolchain-manifest-${target_arch}.json)
  set(bundle_runtime_manifest_file ${bundle_share_dir}/runtime-manifest-${target_arch}.txt)
  set(bundle_compile_config ${bundle_share_dir}/compile-${target_arch}.cfg)
  set(bundle_link_config ${bundle_share_dir}/link-${target_arch}.cfg)
  set(bundle_stamp_file ${bundle_share_dir}/toolchain-${target_arch}.stamp)
  set(bundle_generic_toolchain_file ${bundle_cmake_dir}/ringos-toolchain.cmake)

  set(generated_root ${CMAKE_BINARY_DIR}/generated/installed_toolchain/${target_arch})
  set(toolchain_file_source ${generated_root}/ringos-${target_arch}-toolchain.cmake)
  set(manifest_file_source ${generated_root}/toolchain-manifest-${target_arch}.json)
  set(runtime_manifest_source ${generated_root}/runtime-manifest-${target_arch}.txt)
  set(compile_config_source ${generated_root}/compile-${target_arch}.cfg)
  set(link_config_source ${generated_root}/link-${target_arch}.cfg)
  set(generic_toolchain_source ${generated_root}/ringos-toolchain.cmake)

  file(MAKE_DIRECTORY ${generated_root})

  if(target_arch STREQUAL "x64")
    set(system_processor x86_64)
  elseif(target_arch STREQUAL "arm64")
    set(system_processor aarch64)
  else()
    message(FATAL_ERROR "Unsupported ringos native target architecture: ${target_arch}")
  endif()

  set(compile_config_lines
    --target=${native_target_triple}
    -fno-stack-protector
    -fno-builtin
    -resource-dir
    <CFGDIR>/../../lib/clang/${clang_resource_version}
    -I
    <CFGDIR>/../../sysroots/${native_target_triple}/include)
  string(JOIN "\n" compile_config_contents ${compile_config_lines})
  string(APPEND compile_config_contents "\n")

  ringos_append_sdk_link_flags(${target_arch} link_config_lines)
  list(INSERT link_config_lines 0 --target=${native_target_triple})
  list(APPEND link_config_lines
    -resource-dir
    <CFGDIR>/../../lib/clang/${clang_resource_version}
    <CFGDIR>/../../sysroots/${native_target_triple}/lib/crt0.obj
    <CFGDIR>/../../sysroots/${native_target_triple}/lib/ringos_c.lib
    <CFGDIR>/../../sysroots/${native_target_triple}/lib/ringos_sdk.lib
    <CFGDIR>/../../sysroots/${native_target_triple}/lib/clang_rt.builtins.lib)
  string(JOIN "\n" link_config_contents ${link_config_lines})
  string(APPEND link_config_contents "\n")

  ringos_append_sdk_link_flags(${target_arch} toolchain_link_flags)
  string(JOIN " " toolchain_link_flags_string ${toolchain_link_flags})

  string(CONCAT toolchain_contents
    "get_filename_component(RINGOS_TOOLCHAIN_ROOT \"\${CMAKE_CURRENT_LIST_DIR}/..\" ABSOLUTE)\n"
    "get_filename_component(RINGOS_SHARE_DIR \"\${RINGOS_TOOLCHAIN_ROOT}/share/ringos\" ABSOLUTE)\n"
    "get_filename_component(RINGOS_SYSROOT_DIR \"\${RINGOS_TOOLCHAIN_ROOT}/sysroots/${native_target_triple}\" ABSOLUTE)\n"
    "set(RINGOS_TARGET_ARCH \"${target_arch}\")\n"
    "set(RINGOS_TARGET_TRIPLE \"${native_target_triple}\")\n"
    "set(RINGOS_TOOLCHAIN_VERSION \"${toolchain_version}\")\n"
    "set(RINGOS_CLANG_RESOURCE_DIR \"\${RINGOS_TOOLCHAIN_ROOT}/lib/clang/${clang_resource_version}\")\n"
    "set(RINGOS_SYSROOT_INCLUDE_DIR \"\${RINGOS_SYSROOT_DIR}/include\")\n"
    "set(RINGOS_SYSROOT_CXX_INCLUDE_DIR \"\${RINGOS_SYSROOT_DIR}/include/c++/v1\")\n"
    "set(RINGOS_SYSROOT_LIB_DIR \"\${RINGOS_SYSROOT_DIR}/lib\")\n"
    "set(RINGOS_SYSROOT_CRT0_OBJECT \"\${RINGOS_SYSROOT_LIB_DIR}/crt0.obj\")\n"
    "set(RINGOS_SYSROOT_SDK_LIBRARY \"\${RINGOS_SYSROOT_LIB_DIR}/ringos_sdk.lib\")\n"
    "set(RINGOS_SYSROOT_LIBC_LIBRARY \"\${RINGOS_SYSROOT_LIB_DIR}/ringos_c.lib\")\n"
    "set(RINGOS_SYSROOT_COMPILER_RT_LIBRARY \"\${RINGOS_SYSROOT_LIB_DIR}/clang_rt.builtins.lib\")\n"
    "set(RINGOS_CLANG_COMPILE_CONFIG \"\${RINGOS_SHARE_DIR}/compile-${target_arch}.cfg\")\n"
    "set(RINGOS_CLANG_LINK_CONFIG \"\${RINGOS_SHARE_DIR}/link-${target_arch}.cfg\")\n"
    "set(CMAKE_SYSTEM_NAME Generic)\n"
    "set(CMAKE_SYSTEM_PROCESSOR ${system_processor})\n"
    "set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)\n"
    "set(CMAKE_EXECUTABLE_SUFFIX \".exe\")\n"
    "set(CMAKE_C_COMPILER \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_clang_name}\")\n"
    "set(CMAKE_CXX_COMPILER \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_clangxx_name}\")\n"
    "set(CMAKE_ASM_COMPILER \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_clang_name}\")\n"
    "set(CMAKE_C_COMPILER_TARGET \"\${RINGOS_TARGET_TRIPLE}\")\n"
    "set(CMAKE_CXX_COMPILER_TARGET \"\${RINGOS_TARGET_TRIPLE}\")\n"
    "set(CMAKE_ASM_COMPILER_TARGET \"\${RINGOS_TARGET_TRIPLE}\")\n"
    "set(CMAKE_AR \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_ar_name}\")\n"
    "set(CMAKE_RANLIB \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_ranlib_name}\")\n"
    "set(CMAKE_C_COMPILER_AR \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_ar_name}\")\n"
    "set(CMAKE_C_COMPILER_RANLIB \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_ranlib_name}\")\n"
    "set(CMAKE_CXX_COMPILER_AR \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_ar_name}\")\n"
    "set(CMAKE_CXX_COMPILER_RANLIB \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_ranlib_name}\")\n"
    "set(CMAKE_OBJCOPY \"\${RINGOS_TOOLCHAIN_ROOT}/bin/${toolchain_llvm_objcopy_name}\")\n"
    "set(CMAKE_FIND_ROOT_PATH \"\${RINGOS_SYSROOT_DIR}\")\n"
    "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\n"
    "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\n"
    "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n"
    "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n"
    "set(CMAKE_C_STANDARD_LIBRARIES_INIT \"\")\n"
    "set(CMAKE_CXX_STANDARD_LIBRARIES_INIT \"\")\n"
    "set(CMAKE_ASM_STANDARD_LIBRARIES_INIT \"\")\n"
    "set(CMAKE_C_FLAGS_INIT \"--target=\\\"\${RINGOS_TARGET_TRIPLE}\\\" -fno-stack-protector -fno-builtin -resource-dir \\\"\${RINGOS_CLANG_RESOURCE_DIR}\\\" -I \\\"\${RINGOS_SYSROOT_INCLUDE_DIR}\\\"\")\n"
    "set(CMAKE_CXX_FLAGS_INIT \"--target=\\\"\${RINGOS_TARGET_TRIPLE}\\\" -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-stack-protector -fno-builtin -nostdinc++ -isystem \\\"\${RINGOS_SYSROOT_CXX_INCLUDE_DIR}\\\" -resource-dir \\\"\${RINGOS_CLANG_RESOURCE_DIR}\\\" -I \\\"\${RINGOS_SYSROOT_INCLUDE_DIR}\\\"\")\n"
    "set(CMAKE_ASM_FLAGS_INIT \"--target=\\\"\${RINGOS_TARGET_TRIPLE}\\\" -fno-stack-protector -resource-dir \\\"\${RINGOS_CLANG_RESOURCE_DIR}\\\" -I \\\"\${RINGOS_SYSROOT_INCLUDE_DIR}\\\"\")\n"
    "set(CMAKE_EXE_LINKER_FLAGS_INIT \"--target=\\\"\${RINGOS_TARGET_TRIPLE}\\\" -resource-dir \\\"\${RINGOS_CLANG_RESOURCE_DIR}\\\" ${toolchain_link_flags_string} \\\"\${RINGOS_SYSROOT_CRT0_OBJECT}\\\" \\\"\${RINGOS_SYSROOT_LIBC_LIBRARY}\\\" \\\"\${RINGOS_SYSROOT_SDK_LIBRARY}\\\" \\\"\${RINGOS_SYSROOT_COMPILER_RT_LIBRARY}\\\"\")\n")

  string(CONCAT runtime_manifest_contents
    "toolchain_version=${toolchain_version}\n"
    "target_arch=${target_arch}\n"
    "target_triple=${native_target_triple}\n"
    "sysroot=sysroots/${native_target_triple}\n"
    "crt0=sysroots/${native_target_triple}/lib/crt0.obj\n"
    "sdk=sysroots/${native_target_triple}/lib/ringos_sdk.lib\n"
    "libc=sysroots/${native_target_triple}/lib/ringos_c.lib\n"
    "compiler_rt=sysroots/${native_target_triple}/lib/clang_rt.builtins.lib\n")

  string(CONCAT manifest_contents
    "{\n"
    "  \"toolchain_version\": \"${toolchain_version}\",\n"
    "  \"target_arch\": \"${target_arch}\",\n"
    "  \"target_triple\": \"${native_target_triple}\",\n"
    "  \"llvm_ref\": \"${llvm_ref}\",\n"
    "  \"clang_resource_version\": \"${clang_resource_version}\",\n"
    "  \"sysroot\": \"sysroots/${native_target_triple}\"\n"
    "}\n")

  string(CONCAT generic_toolchain_contents
    "if(NOT DEFINED RINGOS_TARGET_ARCH AND DEFINED CACHE{RINGOS_TARGET_ARCH})\n"
    "  set(RINGOS_TARGET_ARCH \"$CACHE{RINGOS_TARGET_ARCH}\")\n"
    "endif()\n"
    "list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES RINGOS_TARGET_ARCH)\n"
    "if(NOT DEFINED RINGOS_TARGET_ARCH)\n"
    "  message(FATAL_ERROR \"Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before loading ringos-toolchain.cmake.\")\n"
    "endif()\n"
    "if(RINGOS_TARGET_ARCH STREQUAL \"x64\")\n"
    "  include(\"\${CMAKE_CURRENT_LIST_DIR}/ringos-x64-toolchain.cmake\")\n"
    "elseif(RINGOS_TARGET_ARCH STREQUAL \"arm64\")\n"
    "  include(\"\${CMAKE_CURRENT_LIST_DIR}/ringos-arm64-toolchain.cmake\")\n"
    "else()\n"
    "  message(FATAL_ERROR \"Unsupported RINGOS_TARGET_ARCH: \${RINGOS_TARGET_ARCH}\")\n"
    "endif()\n")

  file(WRITE ${compile_config_source} ${compile_config_contents})
  file(WRITE ${link_config_source} ${link_config_contents})
  file(WRITE ${toolchain_file_source} ${toolchain_contents})
  file(WRITE ${manifest_file_source} ${manifest_contents})
  file(WRITE ${runtime_manifest_source} ${runtime_manifest_contents})
  file(WRITE ${generic_toolchain_source} ${generic_toolchain_contents})

  file(GLOB_RECURSE clang_resource_files LIST_DIRECTORIES FALSE ${clang_resource_dir}/*)

  if(NOT TARGET ${target_name})
    add_custom_command(
      OUTPUT ${bundle_stamp_file}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_root}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_bin_dir}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_lib_dir}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_lib_dir}/clang
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_share_dir}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_cmake_dir}
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${bundle_sysroot_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${RINGOS_SDK_SYSROOT_DIR} ${bundle_sysroot_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${clang_resource_dir} ${bundle_resource_dir}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_TOOLCHAIN_CLANG} ${bundle_bin_dir}/${toolchain_clang_name}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_TOOLCHAIN_CLANGXX} ${bundle_bin_dir}/${toolchain_clangxx_name}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_TOOLCHAIN_LLD_LINK} ${bundle_bin_dir}/${toolchain_lld_link_name}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_TOOLCHAIN_LLVM_AR} ${bundle_bin_dir}/${toolchain_llvm_ar_name}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_TOOLCHAIN_LLVM_RANLIB} ${bundle_bin_dir}/${toolchain_llvm_ranlib_name}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RINGOS_TOOLCHAIN_LLVM_OBJCOPY} ${bundle_bin_dir}/${toolchain_llvm_objcopy_name}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${compile_config_source} ${bundle_compile_config}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${link_config_source} ${bundle_link_config}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${toolchain_file_source} ${bundle_toolchain_file}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${manifest_file_source} ${bundle_manifest_file}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${runtime_manifest_source} ${bundle_runtime_manifest_file}
      COMMAND ${CMAKE_COMMAND} -E touch ${bundle_stamp_file}
      DEPENDS
        ${RINGOS_SDK_SYSROOT_TARGET}
        ${RINGOS_SDK_SYSROOT_LIBRARY}
        ${RINGOS_SDK_COMPILE_CONFIG}
        ${RINGOS_SDK_LINK_CONFIG}
        ${toolchain_input_files}
        ${RINGOS_TOOLCHAIN_CLANG}
        ${RINGOS_TOOLCHAIN_CLANGXX}
        ${RINGOS_TOOLCHAIN_LLD_LINK}
        ${RINGOS_TOOLCHAIN_LLVM_AR}
        ${RINGOS_TOOLCHAIN_LLVM_RANLIB}
        ${RINGOS_TOOLCHAIN_LLVM_OBJCOPY}
        ${clang_resource_files}
        ${compile_config_source}
        ${link_config_source}
        ${toolchain_file_source}
        ${manifest_file_source}
        ${runtime_manifest_source}
      BYPRODUCTS
        ${bundle_toolchain_file}
        ${bundle_manifest_file}
        ${bundle_runtime_manifest_file}
      COMMENT "Installing cached ringos ${target_arch} toolchain bundle under ${bundle_root}"
      VERBATIM
    )

    add_custom_target(${target_name} DEPENDS ${bundle_stamp_file})
  endif()

  if(target_arch STREQUAL "x64")
    set(dispatch_target ringos_installed_toolchain_dispatcher)
    set(dispatch_stamp_file ${bundle_share_dir}/toolchain-dispatcher.stamp)

    if(NOT TARGET ${dispatch_target})
      add_custom_command(
        OUTPUT ${dispatch_stamp_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${bundle_cmake_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${generic_toolchain_source} ${bundle_generic_toolchain_file}
        COMMAND ${CMAKE_COMMAND} -E touch ${dispatch_stamp_file}
        DEPENDS ${generic_toolchain_source}
        BYPRODUCTS ${bundle_generic_toolchain_file}
        COMMENT "Installing shared ringos toolchain dispatcher under ${bundle_cmake_dir}"
        VERBATIM
      )

      add_custom_target(${dispatch_target} DEPENDS ${dispatch_stamp_file})
    endif()

    add_dependencies(${target_name} ${dispatch_target})
  endif()

  set(${out_target} ${target_name} PARENT_SCOPE)
  set(${out_bundle_root} ${bundle_root} PARENT_SCOPE)
  set(${out_toolchain_file} ${bundle_toolchain_file} PARENT_SCOPE)
endfunction()
