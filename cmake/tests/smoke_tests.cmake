# Smoke tests are registered here after BUILD_TESTING is set by include(CTest).
# These tests are enabled once the kernel targets exist (Phase 3 and Phase 4).

set(RINGOS_SMOKE_TEST_TIMEOUT 20)

if(BUILD_TESTING)
  if(RINGOS_TARGET_ARCH STREQUAL "x64" AND TARGET ringos_x64)
    add_test(
      NAME smoke_x64
      COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-x64.sh
              $<TARGET_FILE:ringos_x64>
    )

    set_tests_properties(smoke_x64 PROPERTIES TIMEOUT ${RINGOS_SMOKE_TEST_TIMEOUT})
  endif()

  if(RINGOS_TARGET_ARCH STREQUAL "arm64" AND TARGET ringos_arm64)
    add_test(
      NAME smoke_arm64
      COMMAND ${CMAKE_SOURCE_DIR}/scripts/test-smoke-arm64.sh
              $<TARGET_FILE:ringos_arm64>
    )

    set_tests_properties(smoke_arm64 PROPERTIES TIMEOUT ${RINGOS_SMOKE_TEST_TIMEOUT})
  endif()
endif()
