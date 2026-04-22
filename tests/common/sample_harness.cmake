set(RINGOS_SAMPLE_PROJECT_TARGETS
  hello_world
  hello_world_cpp
)

set(RINGOS_SAMPLE_LANE_IDS
  x64-native
  arm64-native
  arm64-x64-emulator
)

function(ringos_get_sample_kernel_suffix out_var sample_target)
  if(sample_target STREQUAL "hello_world")
    set(${out_var} "" PARENT_SCOPE)
  else()
    set(${out_var} "_${sample_target}" PARENT_SCOPE)
  endif()
endfunction()

function(ringos_get_sample_binary_stem out_var sample_target sample_target_arch)
  set(${out_var} "ringos_${sample_target}_${sample_target_arch}_pe64_image" PARENT_SCOPE)
endfunction()

function(ringos_get_sample_script_path out_var sample_target)
  string(REPLACE "_" "-" sample_script_stem "${sample_target}")
  set(${out_var}
    "${CMAKE_SOURCE_DIR}/user/samples/${sample_target}/test-${sample_script_stem}.sh"
    PARENT_SCOPE)
endfunction()

function(ringos_get_sample_smoke_test_name out_var sample_target lane)
  string(REPLACE "-" "_" lane_name "${lane}")
  set(${out_var} "sample_${sample_target}_${lane_name}" PARENT_SCOPE)
endfunction()

function(ringos_resolve_sample_project_args out_var sample_target sample_target_arch)
  string(TOUPPER "${sample_target}" sample_target_upper)
  string(REGEX REPLACE "[^A-Z0-9]" "_" sample_target_upper "${sample_target_upper}")
  string(TOUPPER "${sample_target_arch}" sample_target_arch_upper)
  set(binary_override_var "RINGOS_TEST_APP_${sample_target_upper}_${sample_target_arch_upper}_BINARY")

  set(sample_args
    PROJECT_PATH ${CMAKE_SOURCE_DIR}/user/samples/${sample_target}
    PROJECT_TARGET ${sample_target}
    PROJECT_DEPENDENCY_PATHS
      ${CMAKE_SOURCE_DIR}/kernelsdk)

  if(DEFINED ${binary_override_var} AND NOT "${${binary_override_var}}" STREQUAL "")
    set(sample_args BINARY_PATH ${${binary_override_var}})
  endif()

  set(${out_var} "${sample_args}" PARENT_SCOPE)
endfunction()

function(ringos_add_x64_sample_kernel_target sample_target)
  ringos_get_sample_kernel_suffix(kernel_suffix "${sample_target}")
  ringos_get_sample_binary_stem(binary_stem "${sample_target}" x64)
  ringos_resolve_sample_project_args(sample_args "${sample_target}" x64)

  set(dependency_target "ringos_x64${kernel_suffix}_test_app")

  ringos_add_embedded_x64_test_app(
    ${dependency_target}
    elf64-x86-64
    i386:x86-64
    sample_image_object
    BINARY_STEM ${binary_stem}
    ${sample_args})

  ringos_add_x64_kernel_target(
    ringos_x64${kernel_suffix}
    ${dependency_target}
    ${sample_image_object}
    ${binary_stem})
endfunction()

function(ringos_add_arm64_sample_kernel_target sample_target)
  ringos_get_sample_kernel_suffix(kernel_suffix "${sample_target}")
  ringos_get_sample_binary_stem(binary_stem "${sample_target}" arm64)
  ringos_resolve_sample_project_args(sample_args "${sample_target}" arm64)

  set(dependency_target "ringos_arm64${kernel_suffix}_test_app")

  ringos_add_embedded_arm64_test_app(
    ${dependency_target}
    elf64-littleaarch64
    aarch64
    sample_image_object
    BINARY_STEM ${binary_stem}
    ${sample_args})

  ringos_add_arm64_kernel_target(
    ringos_arm64${kernel_suffix}
    ${dependency_target}
    ${sample_image_object}
    ${binary_stem})
endfunction()

function(ringos_add_arm64_x64_emulator_sample_kernel_target sample_target)
  ringos_get_sample_kernel_suffix(kernel_suffix "${sample_target}")
  ringos_get_sample_binary_stem(binary_stem "${sample_target}" x64)
  ringos_resolve_sample_project_args(sample_args "${sample_target}" x64)

  set(dependency_target "ringos_arm64_x64_emulator${kernel_suffix}_test_app")

  ringos_add_embedded_x64_test_app(
    ${dependency_target}
    elf64-littleaarch64
    aarch64
    sample_image_object
    BINARY_STEM ${binary_stem}
    ${sample_args})

  ringos_add_arm64_kernel_target(
    ringos_arm64_x64_emulator${kernel_suffix}
    ${dependency_target}
    ${sample_image_object}
    ${binary_stem})
endfunction()

function(ringos_add_sample_smoke_test_lanes sample_target)
  ringos_get_sample_script_path(script_path "${sample_target}")

  foreach(lane IN LISTS RINGOS_SAMPLE_LANE_IDS)
    ringos_get_sample_smoke_test_name(test_name "${sample_target}" "${lane}")

    ringos_add_sample_smoke_test(
      ${test_name}
      ${script_path}
      ${lane})
  endforeach()
endfunction()
