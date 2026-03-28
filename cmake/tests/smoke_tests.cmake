if(RINGOS_TARGET_ARCH STREQUAL "x64")
  add_test(
    NAME smoke_x64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-x64.sh
            $<TARGET_FILE:ringos_x64>
  )

  add_test(
    NAME debug_launch_x64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-debug-launch-x64.sh
  )

  set_tests_properties(
    smoke_x64
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    debug_launch_x64
    PROPERTIES
      TIMEOUT 5
  )
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  add_test(
    NAME smoke_arm64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-arm64.sh
            $<TARGET_FILE:ringos_arm64>
  )

  add_test(
    NAME debug_launch_arm64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-debug-launch-arm64.sh
  )

  add_test(
    NAME semihost_arm64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-semihost-arm64.sh
            $<TARGET_FILE:ringos_arm64>
  )

  set_tests_properties(
    smoke_arm64
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    debug_launch_arm64
    PROPERTIES
      TIMEOUT 5
  )

  set_tests_properties(
    semihost_arm64
    PROPERTIES
      TIMEOUT 20
  )
endif()
