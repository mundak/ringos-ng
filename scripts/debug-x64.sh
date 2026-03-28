#!/usr/bin/env bash
# Launches the x64 kernel image in QEMU with the GDB stub enabled and
# execution paused at startup.
#
# Usage:
#   scripts/debug-x64.sh <path-to-ringos_x64>
#
# In a separate terminal, connect with:
#   gdb-multiarch -ex "target remote :1234" <path-to-ringos_x64.elf64>
#
# x64 debug-host logging is enabled through QEMU's x86 debug console on port
# 0xe9 while the kernel exposes a single debug_semihost_log() API.
#
# Optional environment:
#   RINGOS_GDB_PORT  Override the GDB stub port (default: 1234)
#   RINGOS_DEBUGCON  Override the debug console sink (default:
#                    file:/dev/stderr)
#   RINGOS_QEMU_BIN  Override the QEMU binary path for testing/tooling

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_x64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
GDB_PORT="${RINGOS_GDB_PORT:-1234}"
DEBUGCON_DESTINATION="${RINGOS_DEBUGCON:-file:/dev/stderr}"
QEMU_BIN="${RINGOS_QEMU_BIN:-qemu-system-x86_64}"

exec "${QEMU_BIN}" \
  -kernel "${KERNEL_IMAGE}" \
  -display none \
  -debugcon "${DEBUGCON_DESTINATION}" \
  -global isa-debugcon.iobase=0xe9 \
  -monitor none \
  -no-reboot \
  -gdb "tcp::${GDB_PORT}" \
  -S
