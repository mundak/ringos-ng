get_filename_component(repo_root ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
set(CMAKE_SOURCE_DIR ${repo_root})

include(${CMAKE_CURRENT_LIST_DIR}/ringos_toolchain_identity.cmake)

ringos_compute_installed_toolchain_id(x64 x64_toolchain_id_prefix x64_toolchain_id)
ringos_compute_installed_toolchain_id(arm64 arm64_toolchain_id_prefix arm64_toolchain_id)
ringos_compute_bundle_id(${x64_toolchain_id_prefix} ${arm64_toolchain_id_prefix} bundle_id)

message("x64_toolchain_id=${x64_toolchain_id_prefix}")
message("arm64_toolchain_id=${arm64_toolchain_id_prefix}")
message("bundle_id=${bundle_id}")
message("release_tag=ringos-toolchain-${bundle_id}")
