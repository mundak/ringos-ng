#!/usr/bin/env bash
# Smoke test for the arm64 kernel image.
#
# Launches the kernel in QEMU virt, enforces a hard timeout, and asserts that
# the expected serial output appears. Exits non-zero on timeout, early crash,
# or missing output.
#
# Usage:
#   scripts/test-smoke-arm64.sh <path-to-ringos_arm64>
#
# This script is registered as the smoke_arm64 CTest test.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_arm64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
TIMEOUT_SECONDS=15

SERIAL_LOG="$(mktemp)"
trap 'rm -f "${SERIAL_LOG}"' EXIT

timeout "${TIMEOUT_SECONDS}" \
  qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a57 \
    -kernel "${KERNEL_IMAGE}" \
    -serial file:"${SERIAL_LOG}" \
    -display none \
    -no-reboot \
  || true

if ! grep -q "ringos arm64" "${SERIAL_LOG}"; then
  echo "FAIL: expected 'ringos arm64' banner not found in serial output" >&2
  echo "--- serial output ---" >&2
  cat "${SERIAL_LOG}" >&2
  exit 1
fi

if ! grep -q "\[debug\] gdb hooks ready" "${SERIAL_LOG}"; then
  echo "FAIL: expected debug hook log not found in serial output" >&2
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

echo "PASS: arm64 smoke test"
