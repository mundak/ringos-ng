set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

include(${CMAKE_SOURCE_DIR}/tests/common/arm64_test_app_image.cmake)
include(${CMAKE_SOURCE_DIR}/tests/common/x64_test_app_image.cmake)

set(RINGOS_ARM64_HELLO_WORLD_TEST_APP_ARGS
  PROJECT_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world
  PROJECT_TARGET hello_world)

if(DEFINED RINGOS_TEST_APP_HELLO_WORLD_ARM64_BINARY AND NOT RINGOS_TEST_APP_HELLO_WORLD_ARM64_BINARY STREQUAL "")
  set(RINGOS_ARM64_HELLO_WORLD_TEST_APP_ARGS
    BINARY_PATH ${RINGOS_TEST_APP_HELLO_WORLD_ARM64_BINARY})
endif()

set(RINGOS_ARM64_HELLO_WORLD_CPP_TEST_APP_ARGS
  PROJECT_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world_cpp
  PROJECT_TARGET hello_world_cpp)

if(DEFINED RINGOS_TEST_APP_HELLO_WORLD_CPP_ARM64_BINARY AND NOT RINGOS_TEST_APP_HELLO_WORLD_CPP_ARM64_BINARY STREQUAL "")
  set(RINGOS_ARM64_HELLO_WORLD_CPP_TEST_APP_ARGS
    BINARY_PATH ${RINGOS_TEST_APP_HELLO_WORLD_CPP_ARM64_BINARY})
endif()

set(RINGOS_ARM64_X64_HELLO_WORLD_TEST_APP_ARGS
  PROJECT_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world
  PROJECT_TARGET hello_world)

if(DEFINED RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY AND NOT RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY STREQUAL "")
  set(RINGOS_ARM64_X64_HELLO_WORLD_TEST_APP_ARGS
    BINARY_PATH ${RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY})
endif()

set(RINGOS_ARM64_X64_HELLO_WORLD_CPP_TEST_APP_ARGS
  PROJECT_PATH ${CMAKE_SOURCE_DIR}/user/samples/hello_world_cpp
  PROJECT_TARGET hello_world_cpp)

if(DEFINED RINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY AND NOT RINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY STREQUAL "")
  set(RINGOS_ARM64_X64_HELLO_WORLD_CPP_TEST_APP_ARGS
    BINARY_PATH ${RINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY})
endif()

ringos_add_embedded_arm64_test_app(
  ringos_arm64_hello_world_test_app
  elf64-littleaarch64
  aarch64
  ARM64_HELLO_WORLD_IMAGE_OBJECT
  BINARY_STEM ringos_hello_world_arm64_pe64_image
  ${RINGOS_ARM64_HELLO_WORLD_TEST_APP_ARGS})

ringos_add_embedded_arm64_test_app(
  ringos_arm64_hello_world_cpp_test_app
  elf64-littleaarch64
  aarch64
  ARM64_HELLO_WORLD_CPP_IMAGE_OBJECT
  BINARY_STEM ringos_hello_world_cpp_arm64_pe64_image
  ${RINGOS_ARM64_HELLO_WORLD_CPP_TEST_APP_ARGS})

ringos_add_embedded_x64_test_app(
  ringos_arm64_x64_hello_world_test_app
  elf64-littleaarch64
  aarch64
  X64_HELLO_WORLD_IMAGE_OBJECT
  BINARY_STEM ringos_hello_world_x64_pe64_image
  ${RINGOS_ARM64_X64_HELLO_WORLD_TEST_APP_ARGS})

ringos_add_embedded_x64_test_app(
  ringos_arm64_x64_hello_world_cpp_test_app
  elf64-littleaarch64
  aarch64
  X64_HELLO_WORLD_CPP_IMAGE_OBJECT
  BINARY_STEM ringos_hello_world_cpp_x64_pe64_image
  ${RINGOS_ARM64_X64_HELLO_WORLD_CPP_TEST_APP_ARGS})

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
    user_runtime.cpp
    user_runtime.S
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
      ringos_x64_win32_emulation
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

target_compile_options(ringos_x64_win32_emulation
  PRIVATE
    ${ARM64_KERNEL_COMPILE_OPTIONS}
)

ringos_add_arm64_kernel_target(
  ringos_arm64
  ringos_arm64_hello_world_test_app
  ${ARM64_HELLO_WORLD_IMAGE_OBJECT}
  ringos_hello_world_arm64_pe64_image)

ringos_add_arm64_kernel_target(
  ringos_arm64_hello_world_cpp
  ringos_arm64_hello_world_cpp_test_app
  ${ARM64_HELLO_WORLD_CPP_IMAGE_OBJECT}
  ringos_hello_world_cpp_arm64_pe64_image)

ringos_add_arm64_kernel_target(
  ringos_arm64_x64_emulator
  ringos_arm64_x64_hello_world_test_app
  ${X64_HELLO_WORLD_IMAGE_OBJECT}
  ringos_hello_world_x64_pe64_image)

ringos_add_arm64_kernel_target(
  ringos_arm64_x64_emulator_hello_world_cpp
  ringos_arm64_x64_hello_world_cpp_test_app
  ${X64_HELLO_WORLD_CPP_IMAGE_OBJECT}
  ringos_hello_world_cpp_x64_pe64_image)
