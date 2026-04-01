#!/usr/bin/env bash
# Smoke test for the arm64 kernel image that boots the x64 console_service_write sample through the emulator.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_arm64_x64_emulator_console_service_write>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
TIMEOUT_SECONDS=15

DEBUG_LOG="$(mktemp)"
trap 'rm -f "${DEBUG_LOG}"' EXIT

timeout "${TIMEOUT_SECONDS}" \
  "$(dirname "${BASH_SOURCE[0]}")/../scripts/run-arm64.sh" "${KERNEL_IMAGE}" >"${DEBUG_LOG}" 2>&1 \
  || true

for expected_line in \
  "[gdb] ringos arm64" \
  "[gdb] gdb hooks ready" \
  "[gdb] hello world" \
  "[gdb] arm64 x64 emulator runtime ready" \
  "hello from console service sample"; do
  if ! grep -Fq -- "${expected_line}" "${DEBUG_LOG}"; then
    echo "FAIL: expected '${expected_line}' not found in arm64 x64-emulator console_service_write debug output" >&2
    echo "--- debug output ---" >&2
    cat "${DEBUG_LOG}" >&2
    exit 1
  fi
done

echo "PASS: arm64 x64-emulator console_service_write sample test"
