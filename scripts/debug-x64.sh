#!/usr/bin/env bash
# Launch the x64 kernel image in QEMU with GDB stub enabled, paused at startup.
# Connect with: gdb-multiarch -ex "target remote :1234" <kernel-image>
# Usage: debug-x64.sh <kernel-image>
set -euo pipefail

KERNEL_IMAGE="${1:?Usage: $0 <kernel-image>}"

exec qemu-system-x86_64 \
  -machine pc \
  -cpu qemu64 \
  -m 128M \
  -serial stdio \
  -display none \
  -no-reboot \
  -S \
  -gdb tcp::1234 \
  -kernel "${KERNEL_IMAGE}"
