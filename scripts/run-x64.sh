#!/usr/bin/env bash
# Launch the x64 kernel image in QEMU with serial output redirected to stdio.
# Usage: run-x64.sh <kernel-image>
set -euo pipefail

KERNEL_IMAGE="${1:?Usage: $0 <kernel-image>}"

exec qemu-system-x86_64 \
  -machine pc \
  -cpu qemu64 \
  -m 128M \
  -serial stdio \
  -display none \
  -no-reboot \
  -kernel "${KERNEL_IMAGE}"
