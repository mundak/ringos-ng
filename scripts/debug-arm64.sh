#!/usr/bin/env bash
# Launches the arm64 kernel image in QEMU virt with the GDB stub enabled and
# execution paused at startup.
#
# Usage:
#   scripts/debug-arm64.sh <path-to-ringos_arm64>
#
# In a separate terminal, connect with:
#   gdb-multiarch -ex "target remote :1234" <path-to-ringos_arm64>
#
# Optional environment:
#   RINGOS_GDB_PORT  Override the GDB stub port (default: 1234)
#   RINGOS_QEMU_BIN  Override the QEMU binary path for testing/tooling

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_arm64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
GDB_PORT="${RINGOS_GDB_PORT:-1234}"
QEMU_BIN="${RINGOS_QEMU_BIN:-qemu-system-aarch64}"

exec "${QEMU_BIN}" \
  -machine virt \
  -cpu cortex-a57 \
  -kernel "${KERNEL_IMAGE}" \
  -serial stdio \
  -display none \
  -no-reboot \
  -gdb "tcp::${GDB_PORT}" \
  -S
