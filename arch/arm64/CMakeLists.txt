set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

add_executable(ringos_arm64
  boot.S
  debug.cpp
  entry.cpp
)

target_link_options(ringos_arm64
  PRIVATE
    -T ${LINKER_SCRIPT}
)

target_link_libraries(ringos_arm64
  PRIVATE
    ringos_kernel
)

set_target_properties(ringos_arm64
  PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)
