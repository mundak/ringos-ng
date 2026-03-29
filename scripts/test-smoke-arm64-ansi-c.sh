#!/usr/bin/env bash
# Smoke test for the native arm64 kernel image that boots the ANSI C hello world app.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_arm64_ansi_c>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
TIMEOUT_SECONDS=15

DEBUG_LOG="$(mktemp)"
trap 'rm -f "${DEBUG_LOG}"' EXIT

timeout "${TIMEOUT_SECONDS}" \
  "$(dirname "${BASH_SOURCE[0]}")/run-arm64.sh" "${KERNEL_IMAGE}" >"${DEBUG_LOG}" 2>&1 \
  || true

for expected_line in \
  "[gdb] ringos arm64" \
  "[gdb] gdb hooks ready" \
  "[gdb] hello world" \
  "[gdb] arm64 initial user runtime ready" \
  "[gdb] hello world from ANSI C"; do
  if ! grep -Fq -- "${expected_line}" "${DEBUG_LOG}"; then
    echo "FAIL: expected '${expected_line}' not found in arm64 ANSI C debug output" >&2
    echo "--- debug output ---" >&2
    cat "${DEBUG_LOG}" >&2
    exit 1
  fi
done

echo "PASS: arm64 ANSI C smoke test"
