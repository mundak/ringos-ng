#!/usr/bin/env bash
# Launches the x64 kernel image in QEMU with debug-host output routed through
# QEMU's x86 debug console.
#
# Usage:
#   scripts/run-x64.sh <path-to-ringos_x64>
#
# To pause at startup for a GDB attach, use scripts/debug-x64.sh instead.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_x64>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
DEBUGCON_DESTINATION="${RINGOS_DEBUGCON:-stdio}"

exec qemu-system-x86_64 \
  -kernel "${KERNEL_IMAGE}" \
  -display none \
  -debugcon "${DEBUGCON_DESTINATION}" \
  -global isa-debugcon.iobase=0xe9 \
  -monitor none \
  -no-reboot
