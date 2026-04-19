find_program(RINGOS_HOST_BASH NAMES bash REQUIRED)

include(${CMAKE_SOURCE_DIR}/tests/common/sample_harness.cmake)

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

foreach(sample_target IN LISTS RINGOS_SAMPLE_PROJECT_TARGETS)
  ringos_add_sample_smoke_test_lanes(${sample_target})
endforeach()

