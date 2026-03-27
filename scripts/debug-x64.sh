#!/usr/bin/env bash
# Launches the x64 kernel image in QEMU with the GDB stub enabled and
# execution paused at startup.
#
# Usage:
#   scripts/debug-x64.sh <path-to-ringos_x64>
#
# In a separate terminal, connect with:
#   gdb-multiarch -ex "target remote :1234" <path-to-ringos_x64>

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_x64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"

exec qemu-system-x86_64 \
  -kernel "${KERNEL_IMAGE}" \
  -display none \
  -serial stdio \
  -monitor none \
  -no-reboot \
  -s \
  -S
