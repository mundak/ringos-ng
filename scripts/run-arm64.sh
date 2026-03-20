#!/usr/bin/env bash
# Launch the arm64 kernel image in QEMU virt with serial output redirected to stdio.
# Usage: run-arm64.sh <kernel-image>
set -euo pipefail

KERNEL_IMAGE="${1:?Usage: $0 <kernel-image>}"

exec qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -m 128M \
  -serial stdio \
  -display none \
  -no-reboot \
  -kernel "${KERNEL_IMAGE}"
