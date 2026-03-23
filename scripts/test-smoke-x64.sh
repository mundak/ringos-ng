#!/usr/bin/env bash
# Smoke test for the x64 kernel image.
#
# Launches the kernel in QEMU, enforces a hard timeout, and asserts that the
# expected serial output appears. Exits non-zero on timeout, early crash, or
# missing output.
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

SERIAL_LOG="$(mktemp)"
trap 'rm -f "${SERIAL_LOG}"' EXIT

timeout "${TIMEOUT_SECONDS}" \
  qemu-system-x86_64 \
    -kernel "${KERNEL_IMAGE}" \
    -display none \
    -serial "file:${SERIAL_LOG}" \
    -monitor none \
    -no-reboot \
  || true

if ! grep -q "ringos x64" "${SERIAL_LOG}"; then
  echo "FAIL: expected 'ringos x64' banner not found in serial output" >&2
  echo "--- serial output ---" >&2
  cat "${SERIAL_LOG}" >&2
  exit 1
fi

if ! grep -q "hello world" "${SERIAL_LOG}"; then
  echo "FAIL: expected 'hello world' not found in serial output" >&2
  echo "--- serial output ---" >&2
  cat "${SERIAL_LOG}" >&2
  exit 1
fi

echo "PASS: x64 smoke test"
