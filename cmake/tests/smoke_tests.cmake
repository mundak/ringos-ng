if(RINGOS_TARGET_ARCH STREQUAL "x64")
  add_test(
    NAME smoke_x64_native
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-x64.sh
            $<TARGET_FILE:ringos_x64>
  )

  set_tests_properties(
    smoke_x64_native
    PROPERTIES
      TIMEOUT 20
  )
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  add_test(
    NAME smoke_arm64_native
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-arm64-native.sh
      $<TARGET_FILE:ringos_arm64>
  )

  add_test(
    NAME smoke_arm64_x64_emulator
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-arm64-x64-emulator.sh
      $<TARGET_FILE:ringos_arm64_x64_emulator>
  )

  set_tests_properties(
    smoke_arm64_native
    PROPERTIES
      TIMEOUT 20
  )

  set_tests_properties(
    smoke_arm64_x64_emulator
    PROPERTIES
      TIMEOUT 20
  )
endif()
