#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

default_install_root()
{
  if [[ -n "${LOCALAPPDATA:-}" ]]; then
    printf '%s\n' "${LOCALAPPDATA}/ringos/native-llvm-toolchain"
  elif [[ -n "${HOME:-}" ]]; then
    printf '%s\n' "${HOME}/.cache/ringos/native-llvm-toolchain"
  else
    printf '%s\n' "${REPO_ROOT}/build/native-llvm-toolchain"
  fi
}

LLVM_REPO_URL="${RINGOS_LLVM_REPO_URL:-https://github.com/llvm/llvm-project.git}"
LLVM_REF="${RINGOS_LLVM_REF:-llvmorg-18.1.8}"
LLVM_ROOT="${RINGOS_LLVM_ROOT:-${REPO_ROOT}/tools/llvm}"
LLVM_SOURCE_DIR="${RINGOS_LLVM_SOURCE_DIR:-${LLVM_ROOT}/src/llvm-project}"
LLVM_BUILD_DIR="${RINGOS_LLVM_BUILD_DIR:-${LLVM_ROOT}/build}"
LLVM_INSTALL_DIR="${RINGOS_LLVM_INSTALL_DIR:-$(default_install_root)}"
LLVM_PATCH_DIR="${RINGOS_LLVM_PATCH_DIR:-${LLVM_ROOT}/patches}"
LLVM_PROJECTS="${RINGOS_LLVM_ENABLE_PROJECTS:-clang;lld}"
LLVM_RUNTIMES="${RINGOS_LLVM_ENABLE_RUNTIMES:-compiler-rt}"
LLVM_TARGETS="${RINGOS_LLVM_TARGETS_TO_BUILD:-AArch64;X86}"

mkdir -p "${LLVM_ROOT}" "$(dirname "${LLVM_SOURCE_DIR}")" "$(dirname "${LLVM_INSTALL_DIR}")"

if [[ ! -d "${LLVM_SOURCE_DIR}/.git" ]]; then
  git clone "${LLVM_REPO_URL}" "${LLVM_SOURCE_DIR}"
fi

git -C "${LLVM_SOURCE_DIR}" fetch --tags --force origin
git -C "${LLVM_SOURCE_DIR}" checkout --detach "${LLVM_REF}"

if [[ -d "${LLVM_PATCH_DIR}" ]]; then
  shopt -s nullglob
  patch_files=("${LLVM_PATCH_DIR}"/*.patch)
  shopt -u nullglob

  for patch_file in "${patch_files[@]}"; do
    if git -C "${LLVM_SOURCE_DIR}" apply --check "${patch_file}"; then
      git -C "${LLVM_SOURCE_DIR}" apply "${patch_file}"
    elif git -C "${LLVM_SOURCE_DIR}" apply --reverse --check "${patch_file}"; then
      echo "Skipping already-applied patch ${patch_file}"
    else
      echo "Failed to apply patch ${patch_file}" >&2
      exit 1
    fi
  done
fi

cmake -S "${LLVM_SOURCE_DIR}/llvm" -B "${LLVM_BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LLVM_INSTALL_DIR}" \
  -DLLVM_ENABLE_PROJECTS="${LLVM_PROJECTS}" \
  -DLLVM_ENABLE_RUNTIMES="${LLVM_RUNTIMES}" \
  -DLLVM_TARGETS_TO_BUILD="${LLVM_TARGETS}" \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DLLVM_INCLUDE_TESTS=OFF

cmake --build "${LLVM_BUILD_DIR}" --target install

echo "Installed LLVM toolchain under ${LLVM_INSTALL_DIR}"
echo "clang: ${LLVM_INSTALL_DIR}/bin/clang"
echo "lld-link: ${LLVM_INSTALL_DIR}/bin/lld-link"
echo "ringos sysroot root: ${LLVM_INSTALL_DIR}/sysroot"
