#!/usr/bin/env bash
# End-to-end test for x64 debug-host output routed through QEMU's debug console.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_x64>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_IMAGE="$1"
SYMBOL_IMAGE="${KERNEL_IMAGE}.elf64"
GDB_PORT="${RINGOS_TEST_GDB_PORT:-4321}"

SERIAL_LOG="$(mktemp)"
DEBUGCON_LOG="$(mktemp)"
GDB_LOG="$(mktemp)"

cleanup()
{
  if [[ -n "${QEMU_PID:-}" ]]; then
    kill "${QEMU_PID}" >/dev/null 2>&1 || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi

  rm -f "${SERIAL_LOG}" "${DEBUGCON_LOG}" "${GDB_LOG}"
}

trap cleanup EXIT

RINGOS_GDB_PORT="${GDB_PORT}" \
RINGOS_DEBUGCON="file:${DEBUGCON_LOG}" \
  "${SCRIPT_DIR}/debug-x64.sh" "${KERNEL_IMAGE}" >"${SERIAL_LOG}" 2>&1 &
QEMU_PID=$!

sleep 1

gdb_exit_code=0

if ! timeout 10 \
  gdb-multiarch \
    -batch \
    -ex "set pagination off" \
    -ex "set confirm off" \
    -ex "target remote :${GDB_PORT}" \
    -ex "break kernel_main" \
    -ex "continue" \
    -ex 'set $pc = debug_semihost_self_test' \
    -ex "continue" \
    "${SYMBOL_IMAGE}" >"${GDB_LOG}" 2>&1; then
  gdb_exit_code=$?
fi

if ! grep -Fq -- "[gdb] semihosting self-test" "${DEBUGCON_LOG}"; then
  echo "FAIL: expected x64 debug-host output was not observed on the QEMU debug console" >&2
  echo "GDB exit code: ${gdb_exit_code}" >&2
  echo "--- debug console output ---" >&2
  cat "${DEBUGCON_LOG}" >&2
  echo "--- gdb output ---" >&2
  cat "${GDB_LOG}" >&2
  echo "--- qemu serial output ---" >&2
  cat "${SERIAL_LOG}" >&2
  exit 1
fi

echo "PASS: x64 debug-host logging through QEMU debug console"
