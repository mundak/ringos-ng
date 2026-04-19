#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
build_root="${repo_root}/build/host-unit-tests"
toolchain_root="${repo_root}/build/toolchain"
sdk_root="${repo_root}/build/sdk"
toolchain_file="${toolchain_root}/cmake/ringos-toolchain.cmake"
release_repo="${GITHUB_REPOSITORY:-mundak/ringos-ng}"

rm -rf "${build_root}"

"${repo_root}/tests/download-latest-toolchain.sh" \
  --repo "${release_repo}" \
  --archive-dir "${repo_root}/build" \
  --install-root "${toolchain_root}"

"${repo_root}/tests/download-latest-sdk.sh" \
  --repo "${release_repo}" \
  --archive-dir "${repo_root}/build" \
  --install-root "${sdk_root}"

cmake -S "${repo_root}" \
  -B "${build_root}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRINGOS_TARGET_ARCH=x64 \
  -DRINGOS_ENABLE_TESTING=ON \
  -DRINGOS_INSTALLED_TOOLCHAIN_FILE="${toolchain_file}" \
  -DRINGOS_SDK_ROOT="${sdk_root}"

cmake --build "${build_root}" --target ringos_x64_emulator_unit_tests ringos_x64_win32_loader_unit_tests

ctest --test-dir "${build_root}" --output-on-failure --tests-regex "x64_emulator_unit|x64_win32_loader_unit"
