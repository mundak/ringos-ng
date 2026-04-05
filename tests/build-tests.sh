#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
  echo "Usage: tests/build-tests.sh <sample-dir> <target-arch> <build-name> [cmake-target]" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
sample_dir_input="$1"
target_arch="$2"
build_name="$3"
cmake_target="${4:-hello_world}"
release_repo="${GITHUB_REPOSITORY:-mundak/ringos-ng}"

case "${target_arch}" in
  x64|arm64)
    ;;
  *)
    echo "Unsupported target architecture: ${target_arch}" >&2
    exit 1
    ;;
esac

if [[ "${sample_dir_input}" = /* ]]; then
  sample_dir="${sample_dir_input}"
else
  sample_dir="${repo_root}/${sample_dir_input}"
fi

if [[ ! -f "${sample_dir}/CMakeLists.txt" ]]; then
  echo "Sample directory does not contain CMakeLists.txt: ${sample_dir}" >&2
  exit 1
fi

build_root="${repo_root}/build/sample-tests/${build_name}"
toolchain_root="${repo_root}/build/toolchain"
toolchain_file="${toolchain_root}/cmake/ringos-toolchain.cmake"

rm -rf "${build_root}"

"${repo_root}/tests/download-latest-toolchain.sh" \
  --repo "${release_repo}" \
  --archive-dir "${repo_root}/build" \
  --install-root "${toolchain_root}"

if [[ ! -f "${toolchain_file}" ]]; then
  echo "Installed toolchain file not found: ${toolchain_file}" >&2
  exit 1
fi

cmake -S "${sample_dir}" \
  -B "${build_root}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
  -DRINGOS_TARGET_ARCH="${target_arch}"

cmake --build "${build_root}" --target "${cmake_target}"

if [[ ! -f "${build_root}/${cmake_target}.exe" ]]; then
  echo "Expected executable was not produced: ${build_root}/${cmake_target}.exe" >&2
  exit 1
fi

echo "Built ${cmake_target}.exe for ${target_arch} using the published toolchain bundle"
echo "Build directory: ${build_root}"
echo "Executable: ${build_root}/${cmake_target}.exe"
