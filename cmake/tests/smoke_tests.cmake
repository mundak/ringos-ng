if(RINGOS_TARGET_ARCH STREQUAL "x64")
  add_test(
    NAME smoke_x64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-x64.sh
            $<TARGET_FILE:ringos_x64>
  )

  set_tests_properties(
    smoke_x64
    PROPERTIES
      TIMEOUT 20
  )
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  add_test(
    NAME smoke_arm64
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-arm64.sh
            $<TARGET_FILE:ringos_arm64>
  )

  set_tests_properties(
    smoke_arm64
    PROPERTIES
      TIMEOUT 20
  )
endif()
