#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

default_install_root()
{
  if [[ -n "${LOCALAPPDATA:-}" ]]; then
    printf '%s\n' "${LOCALAPPDATA}/ringos/toolchain"
  elif [[ -n "${HOME:-}" ]]; then
    printf '%s\n' "${HOME}/.cache/ringos/toolchain"
  else
    printf '%s\n' "${REPO_ROOT}/build/installed-toolchain"
  fi
}

usage()
{
  cat <<EOF
Usage: $0 <x64|arm64> [source-path]

Build a hosted C sample against the published ringos installed-toolchain bundle.

Environment:
  RINGOS_TOOLCHAIN_ROOT         Override the extracted toolchain location.
  RINGOS_TOOLCHAIN_RELEASE_REPO Override the GitHub repository that owns the release assets.
  GH_TOKEN or GITHUB_TOKEN      Optional token for private release downloads.
EOF
}

resolve_toolchain_clang()
{
  local toolchain_root="$1"
  local candidate=""

  for candidate in clang clang-18 clang-17; do
    if [[ -x "${toolchain_root}/bin/${candidate}" ]]; then
      printf '%s\n' "${toolchain_root}/bin/${candidate}"
      return 0
    fi
  done

  echo "Unable to find a bundled clang executable under ${toolchain_root}/bin" >&2
  exit 1
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage >&2
  exit 1
fi

TARGET_ARCH="$1"
SOURCE_PATH="${2:-${REPO_ROOT}/user/samples/test_app.c}"
TOOLCHAIN_ROOT="${RINGOS_TOOLCHAIN_ROOT:-$(default_install_root)}"
RELEASE_REPO="${RINGOS_TOOLCHAIN_RELEASE_REPO:-${GITHUB_REPOSITORY:-mundak/ringos-ng}}"

case "${TARGET_ARCH}" in
  x64)
    TARGET_TRIPLE="x86_64-unknown-ringos"
    COMPILE_CONFIG="${TOOLCHAIN_ROOT}/share/ringos/compile-x64.cfg"
    LINK_CONFIG="${TOOLCHAIN_ROOT}/share/ringos/link-x64.cfg"
    ;;
  arm64)
    TARGET_TRIPLE="aarch64-unknown-ringos"
    COMPILE_CONFIG="${TOOLCHAIN_ROOT}/share/ringos/compile-arm64.cfg"
    LINK_CONFIG="${TOOLCHAIN_ROOT}/share/ringos/link-arm64.cfg"
    ;;
  *)
    echo "Unsupported target architecture '${TARGET_ARCH}'" >&2
    usage >&2
    exit 1
    ;;
esac

bash "${REPO_ROOT}/tools/toolchain/ensure-toolchain-release.sh" \
  --repo "${RELEASE_REPO}" \
  --install-root "${TOOLCHAIN_ROOT}"

if [[ ! -f "${SOURCE_PATH}" ]]; then
  echo "Source file not found: ${SOURCE_PATH}" >&2
  exit 1
fi

if [[ ! -f "${COMPILE_CONFIG}" || ! -f "${LINK_CONFIG}" ]]; then
  echo "Expected installed toolchain config files under ${TOOLCHAIN_ROOT}/share/ringos" >&2
  exit 1
fi

if [[ ! -d "${TOOLCHAIN_ROOT}/sysroots/${TARGET_TRIPLE}" ]]; then
  echo "Expected installed sysroot under ${TOOLCHAIN_ROOT}/sysroots/${TARGET_TRIPLE}" >&2
  exit 1
fi

COMPILER="$(resolve_toolchain_clang "${TOOLCHAIN_ROOT}")"

OUTPUT_DIR="${REPO_ROOT}/build/user-samples/${TARGET_ARCH}"
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

echo "Built hosted C sample with ${COMPILER}: ${OUTPUT_PATH}"
