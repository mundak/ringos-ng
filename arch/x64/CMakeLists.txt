set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

add_executable(ringos_x64
  entry.cpp
)

target_compile_options(ringos_x64
  PRIVATE
    -m64
    -mno-red-zone
)

target_link_options(ringos_x64
  PRIVATE
    -T ${LINKER_SCRIPT}
)

target_link_libraries(ringos_x64
  PRIVATE
    ringos_kernel
)

set_target_properties(ringos_x64
  PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)
