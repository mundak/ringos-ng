#!/usr/bin/env bash
# Launches the arm64 kernel image in QEMU virt with the GDB stub enabled and
# execution paused at startup.
#
# Usage:
#   scripts/debug-arm64.sh <path-to-ringos_arm64>
#
# In a separate terminal, connect with:
#   gdb-multiarch -ex "target remote :1234" <path-to-ringos_arm64>

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
  -no-reboot \
  -s \
  -S
