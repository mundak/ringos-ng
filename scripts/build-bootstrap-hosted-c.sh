#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "Usage: $0 <x64|arm64> [compiler] [source-path]" >&2
  exit 1
fi

TARGET_ARCH="$1"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

COMPILER="${2:-clang}"
SOURCE_PATH="${3:-${REPO_ROOT}/user/samples/test_app.c}"

case "${TARGET_ARCH}" in
  x64)
    CONFIGURE_PRESET="x64-debug"
    BUILD_PRESET="stage-x64-sdk-sysroot"
    TARGET_TRIPLE="x86_64-pc-windows-msvc"
    ;;
  arm64)
    CONFIGURE_PRESET="arm64-debug"
    BUILD_PRESET="stage-arm64-sdk-sysroot"
    TARGET_TRIPLE="aarch64-pc-windows-msvc"
    ;;
  *)
    echo "Unsupported target architecture '${TARGET_ARCH}'" >&2
    exit 1
    ;;
esac

cmake --preset "${CONFIGURE_PRESET}"
cmake --build --preset "${BUILD_PRESET}"

SYSROOT_DIR="${REPO_ROOT}/build/${CONFIGURE_PRESET}/sysroot/${TARGET_TRIPLE}"
COMPILE_CONFIG="${SYSROOT_DIR}/share/ringos/bootstrap-compile.cfg"
LINK_CONFIG="${SYSROOT_DIR}/share/ringos/bootstrap-link.cfg"

if [[ ! -f "${COMPILE_CONFIG}" || ! -f "${LINK_CONFIG}" ]]; then
  echo "Expected staged sysroot config files under ${SYSROOT_DIR}" >&2
  exit 1
fi

OUTPUT_DIR="${REPO_ROOT}/build/stage8-bootstrap/${TARGET_ARCH}"
SOURCE_STEM="$(basename "${SOURCE_PATH}")"
SOURCE_STEM="${SOURCE_STEM%.*}"
OUTPUT_PATH="${OUTPUT_DIR}/${SOURCE_STEM}.exe"

mkdir -p "${OUTPUT_DIR}"

"${COMPILER}" \
  --config="${COMPILE_CONFIG}" \
  --config="${LINK_CONFIG}" \
  -O2 \
  -Wall \
  -Wextra \
  -Wpedantic \
  "${SOURCE_PATH}" \
  -o "${OUTPUT_PATH}"

echo "Built hosted C sample: ${OUTPUT_PATH}"
