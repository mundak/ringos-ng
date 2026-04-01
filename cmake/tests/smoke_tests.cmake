if(RINGOS_TARGET_ARCH STREQUAL "x64")
  add_test(
    NAME sample_hello_world_x64_native
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-sample-hello-world-x64-native.sh
            $<TARGET_FILE:ringos_x64>
  )

  add_test(
    NAME sample_console_service_write_x64_native
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-sample-console-service-write-x64-native.sh
            $<TARGET_FILE:ringos_x64_console_service_write>
  )

  set_tests_properties(
    sample_hello_world_x64_native
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    sample_console_service_write_x64_native
    PROPERTIES
      TIMEOUT 20
  )
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  add_test(
    NAME sample_hello_world_arm64_native
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-sample-hello-world-arm64-native.sh
      $<TARGET_FILE:ringos_arm64>
  )

  add_test(
    NAME sample_console_service_write_arm64_native
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-sample-console-service-write-arm64-native.sh
      $<TARGET_FILE:ringos_arm64_console_service_write>
  )

  add_test(
    NAME sample_hello_world_arm64_x64_emulator
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-sample-hello-world-arm64-x64-emulator.sh
      $<TARGET_FILE:ringos_arm64_x64_emulator>
  )

  add_test(
    NAME sample_console_service_write_arm64_x64_emulator
    COMMAND ${CMAKE_SOURCE_DIR}/tests/test-sample-console-service-write-arm64-x64-emulator.sh
      $<TARGET_FILE:ringos_arm64_x64_emulator_console_service_write>
  )

  set_tests_properties(
    sample_hello_world_arm64_native
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    sample_console_service_write_arm64_native
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    sample_hello_world_arm64_x64_emulator
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    sample_console_service_write_arm64_x64_emulator
    PROPERTIES
      TIMEOUT 20
  )
endif()
