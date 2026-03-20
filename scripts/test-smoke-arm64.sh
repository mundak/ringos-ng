#!/usr/bin/env bash
# Smoke test for the arm64 kernel target.
# Launches QEMU via run-arm64.sh, captures serial output, and asserts that the
# expected strings appear within the timeout window.
# Usage: test-smoke-arm64.sh <kernel-image>
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_IMAGE="${1:?Usage: $0 <kernel-image>}"

TIMEOUT=20
EXPECTED_STRINGS=(
  "ringos arm64"
  "Hello, world!"
)

output=$(timeout "${TIMEOUT}" "${SCRIPT_DIR}/run-arm64.sh" "${KERNEL_IMAGE}" 2>&1 || true)

failed=0
for expected in "${EXPECTED_STRINGS[@]}"; do
  if ! echo "${output}" | grep -qF "${expected}"; then
    echo "FAIL: expected string not found: '${expected}'"
    failed=1
  fi
done

if [[ ${failed} -eq 1 ]]; then
  echo "--- captured output ---"
  echo "${output}"
  echo "-----------------------"
  exit 1
fi

echo "PASS: all expected strings found"
exit 0
