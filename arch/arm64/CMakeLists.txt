set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/tests/common/arm64_test_app_image.cmake)
include(${CMAKE_SOURCE_DIR}/tests/common/x64_test_app_image.cmake)
include(${CMAKE_SOURCE_DIR}/tests/common/sample_harness.cmake)

set(ARM64_KERNEL_COMPILE_OPTIONS
  -mgeneral-regs-only
)

function(
  ringos_add_arm64_kernel_target
  target_name
  client_dependency_target
  client_image_object
  client_runtime_mode)
  add_executable(${target_name}
    machines/qemu_arm64_virt_boot.S
    debug.cpp
    entry.cpp
    machine.cpp
    machines/qemu_arm64_virt_machine.cpp
    ${client_image_object}
    arm64_initial_user_runtime_platform.cpp
    user_runtime.cpp
    user_runtime.S
  )

  target_include_directories(${target_name}
    PRIVATE
      ${CMAKE_SOURCE_DIR}/kernel
      ${CMAKE_SOURCE_DIR}/user/sdk/include
  )

  add_dependencies(${target_name} ${client_dependency_target})
  target_compile_options(${target_name}
    PRIVATE
      ${ARM64_KERNEL_COMPILE_OPTIONS}
  )
  target_link_options(${target_name}
    PRIVATE
      -T ${LINKER_SCRIPT}
      --defsym=_binary_ringos_test_app_image_start=_binary_${client_runtime_mode}_start
      --defsym=_binary_ringos_test_app_image_end=_binary_${client_runtime_mode}_end
  )
  target_link_libraries(${target_name}
    PRIVATE
      ringos_kernel
      ringos_x64_emulator
      ringos_x64_pe_loader
  )
  set_target_properties(${target_name}
    PROPERTIES
      LINK_DEPENDS ${LINKER_SCRIPT}
  )
endfunction()

target_compile_options(ringos_kernel
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

target_compile_options(ringos_x64_emulator
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

target_compile_options(ringos_x64_pe_loader
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

foreach(sample_target IN LISTS RINGOS_SAMPLE_PROJECT_TARGETS)
  ringos_add_arm64_sample_kernel_target(${sample_target})
  ringos_add_arm64_x64_emulator_sample_kernel_target(${sample_target})
endforeach()
