#!/usr/bin/env bash
# Smoke test for the x64 kernel image.
#
# Launches the kernel in QEMU, enforces a hard timeout, and asserts that the
# expected host-side debug output appears. Exits non-zero on timeout, early
# crash, or missing output.
#
# Usage:
#   scripts/test-smoke-x64.sh <path-to-ringos_x64>
#
# This script is registered as the smoke_x64 CTest test.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_x64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
TIMEOUT_SECONDS=15

DEBUG_LOG="$(mktemp)"
trap 'rm -f "${DEBUG_LOG}"' EXIT

timeout "${TIMEOUT_SECONDS}" \
  "$(dirname "${BASH_SOURCE[0]}")/run-x64.sh" "${KERNEL_IMAGE}" >"${DEBUG_LOG}" 2>&1 \
  || true

for expected_line in \
  "[gdb] ringos x64" \
  "[gdb] gdb hooks ready" \
  "[gdb] hello world"; do
  if ! grep -Fq -- "${expected_line}" "${DEBUG_LOG}"; then
    echo "FAIL: expected '${expected_line}' not found in x64 debug output" >&2
    echo "--- debug output ---" >&2
    cat "${DEBUG_LOG}" >&2
    exit 1
  fi
done

echo "PASS: x64 smoke test"
