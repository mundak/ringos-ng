#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

default_cache_root()
{
  if [[ -n "${LOCALAPPDATA:-}" ]]; then
    printf '%s\n' "${LOCALAPPDATA}/ringos/native-llvm-toolchain-local"
  elif [[ -n "${HOME:-}" ]]; then
    printf '%s\n' "${HOME}/.cache/ringos/native-llvm-toolchain-local"
  else
    printf '%s\n' "${repo_root}/build/native-llvm-toolchain-local"
  fi
}

cache_root="${RINGOS_TOOLCHAIN_CACHE_ROOT:-$(default_cache_root)}"
llvm_source_dir="${RINGOS_LLVM_SOURCE_DIR:-${cache_root}/src/llvm-project}"
llvm_build_dir="${RINGOS_LLVM_BUILD_DIR:-${cache_root}/build}"
previous_stage_root="${RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT:-${cache_root}/install}"

mkdir -p "${cache_root}"

export RINGOS_LLVM_SOURCE_DIR="${llvm_source_dir}"
export RINGOS_LLVM_BUILD_DIR="${llvm_build_dir}"
export RINGOS_LLVM_INSTALL_DIR="${previous_stage_root}"
export RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT="${previous_stage_root}"

echo "Reusing local LLVM cache root: ${cache_root}"
echo "LLVM source dir: ${RINGOS_LLVM_SOURCE_DIR}"
echo "LLVM build dir: ${RINGOS_LLVM_BUILD_DIR}"
echo "Previous-stage toolchain root: ${RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT}"

bash "${repo_root}/tools/llvm/build-clang-toolchain.sh"
bash "${repo_root}/tools/toolchain/build-toolchain.sh" "$@"