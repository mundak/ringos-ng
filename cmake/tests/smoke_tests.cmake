find_program(RINGOS_HOST_BASH NAMES bash REQUIRED)

function(ringos_add_sample_smoke_test test_name script_path)
  if(ARGC GREATER 2)
    add_test(
      NAME ${test_name}
      COMMAND ${RINGOS_HOST_BASH} ${script_path} ${ARGN}
    )
  else()
    add_test(
      NAME ${test_name}
      COMMAND ${RINGOS_HOST_BASH} ${script_path}
    )
  endif()

  set_tests_properties(
    ${test_name}
    PROPERTIES
      TIMEOUT 300
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endfunction()

ringos_add_sample_smoke_test(
  sample_hello_world_x64_native
  ${CMAKE_SOURCE_DIR}/user/samples/hello_world/test-hello-world-x64.sh
)

ringos_add_sample_smoke_test(
  sample_hello_world_cpp_x64_native
  ${CMAKE_SOURCE_DIR}/user/samples/hello_world_cpp/test-hello-world-cpp-x64.sh
)

ringos_add_sample_smoke_test(
  sample_hello_world_arm64_native
  ${CMAKE_SOURCE_DIR}/user/samples/hello_world/test-hello-world-arm64.sh
)

ringos_add_sample_smoke_test(
  sample_hello_world_cpp_arm64_native
  ${CMAKE_SOURCE_DIR}/user/samples/hello_world_cpp/test-hello-world-cpp-arm64.sh
)

ringos_add_sample_smoke_test(
  sample_hello_world_arm64_x64_emulator
  ${CMAKE_SOURCE_DIR}/user/samples/hello_world/test-hello-world-x64-on-arm64.sh
)

ringos_add_sample_smoke_test(
  sample_hello_world_cpp_arm64_x64_emulator
  ${CMAKE_SOURCE_DIR}/user/samples/hello_world_cpp/test-hello-world-cpp-x64-on-arm64.sh
)

