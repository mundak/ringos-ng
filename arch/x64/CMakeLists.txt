set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/tests/common/x64_test_app_image.cmake)
include(${CMAKE_SOURCE_DIR}/tests/common/sample_harness.cmake)

function(
  ringos_add_x64_kernel_target
  target_name
  client_dependency_target
  client_image_object
  client_runtime_mode)
  add_executable(${target_name}
    boot.S
    debug.cpp
    entry.cpp
    machine.cpp
    ${client_image_object}
    x64_initial_user_runtime_platform.cpp
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
      -m64
      -mno-red-zone
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

  add_custom_command(
    TARGET ${target_name}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${target_name}>
            $<TARGET_FILE_DIR:${target_name}>/${target_name}.elf64
    COMMAND ${CMAKE_OBJCOPY} -O elf32-i386
            $<TARGET_FILE_DIR:${target_name}>/${target_name}.elf64
            $<TARGET_FILE:${target_name}>
    COMMENT "Converting to Multiboot-compatible 32-bit ELF (original saved as ${target_name}.elf64)"
  )
endfunction()

foreach(sample_target IN LISTS RINGOS_SAMPLE_PROJECT_TARGETS)
  ringos_add_x64_sample_kernel_target(${sample_target})
endforeach()
