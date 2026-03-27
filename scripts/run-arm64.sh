#!/usr/bin/env bash
# Launches the arm64 kernel image in QEMU virt with serial output redirected
# to stdout.
#
# Usage:
#   scripts/run-arm64.sh <path-to-ringos_arm64>
#
# To pause at startup for a GDB attach, use scripts/debug-arm64.sh instead.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_arm64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"

exec qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -kernel "${KERNEL_IMAGE}" \
  -serial stdio \
  -display none \
  -no-reboot
