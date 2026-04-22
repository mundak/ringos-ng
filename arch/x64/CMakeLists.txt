set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/tests/common/x64_test_app_image.cmake)
include(${CMAKE_SOURCE_DIR}/tests/common/sample_harness.cmake)

function(
  ringos_add_x64_kernel_target
  target_name
  client_dependency_target
  client_image_object
  client_runtime_mode)
  set(options)
  set(oneValueArgs SERVICE_DEPENDENCY_TARGET SERVICE_IMAGE_OBJECT SERVICE_RUNTIME_MODE)
  cmake_parse_arguments(RINGOS_X64_KERNEL_TARGET "${options}" "${oneValueArgs}" "" ${ARGN})

  set(kernel_sources
    boot.S
    debug.cpp
    entry.cpp
    machine.cpp
    ${client_image_object}
    x64_initial_user_runtime_platform.cpp
    user_runtime.cpp
    user_runtime.S)

  if(RINGOS_X64_KERNEL_TARGET_SERVICE_IMAGE_OBJECT)
    list(APPEND kernel_sources ${RINGOS_X64_KERNEL_TARGET_SERVICE_IMAGE_OBJECT})
  endif()

  set(service_image_start 0)
  set(service_image_end 0)

  if(RINGOS_X64_KERNEL_TARGET_SERVICE_RUNTIME_MODE)
    set(service_image_start _binary_${RINGOS_X64_KERNEL_TARGET_SERVICE_RUNTIME_MODE}_start)
    set(service_image_end _binary_${RINGOS_X64_KERNEL_TARGET_SERVICE_RUNTIME_MODE}_end)
  endif()

  add_executable(${target_name}
    ${kernel_sources}
  )

  target_include_directories(${target_name}
    PRIVATE
      ${CMAKE_SOURCE_DIR}/kernel
  )

  add_dependencies(${target_name} ${client_dependency_target})

  if(RINGOS_X64_KERNEL_TARGET_SERVICE_DEPENDENCY_TARGET)
    add_dependencies(${target_name} ${RINGOS_X64_KERNEL_TARGET_SERVICE_DEPENDENCY_TARGET})
  endif()

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
        --defsym=_binary_ringos_terminal_service_image_start=${service_image_start}
        --defsym=_binary_ringos_terminal_service_image_end=${service_image_end}
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
