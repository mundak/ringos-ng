#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

LLVM_REPO_URL="${RINGOS_LLVM_REPO_URL:-https://github.com/llvm/llvm-project.git}"
LLVM_REF="${RINGOS_LLVM_REF:-llvmorg-18.1.8}"
LLVM_ROOT="${RINGOS_LLVM_ROOT:-${REPO_ROOT}/tools/llvm}"
LLVM_SOURCE_DIR="${RINGOS_LLVM_SOURCE_DIR:-${LLVM_ROOT}/src/llvm-project}"

mkdir -p "$(dirname "${LLVM_SOURCE_DIR}")"

if [[ ! -d "${LLVM_SOURCE_DIR}/.git" ]]; then
  git clone --filter=blob:none --no-checkout "${LLVM_REPO_URL}" "${LLVM_SOURCE_DIR}"
  git -C "${LLVM_SOURCE_DIR}" sparse-checkout init --cone
fi

git -C "${LLVM_SOURCE_DIR}" sparse-checkout set libcxx/include
git -C "${LLVM_SOURCE_DIR}" fetch --depth 1 origin "${LLVM_REF}"
git -C "${LLVM_SOURCE_DIR}" checkout --detach --force FETCH_HEAD

echo "Prepared libc++ source headers under ${LLVM_SOURCE_DIR}/libcxx/include"
