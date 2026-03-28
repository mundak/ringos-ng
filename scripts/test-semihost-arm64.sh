#!/usr/bin/env bash
# End-to-end test for arm64 semihosting output routed through GDB.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_arm64>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_IMAGE="$1"
GDB_PORT="${RINGOS_TEST_GDB_PORT:-4321}"

SERIAL_LOG="$(mktemp)"
GDB_LOG="$(mktemp)"

cleanup()
{
  if [[ -n "${QEMU_PID:-}" ]]; then
    kill "${QEMU_PID}" >/dev/null 2>&1 || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi

  rm -f "${SERIAL_LOG}" "${GDB_LOG}"
}

trap cleanup EXIT

RINGOS_GDB_PORT="${GDB_PORT}" \
  "${SCRIPT_DIR}/debug-arm64.sh" "${KERNEL_IMAGE}" >"${SERIAL_LOG}" 2>&1 &
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
    "${KERNEL_IMAGE}" >"${GDB_LOG}" 2>&1; then
  gdb_exit_code=$?
fi

if ! grep -Fq -- "[gdb] semihosting self-test" "${GDB_LOG}"; then
  echo "FAIL: expected semihosting output was not observed in GDB" >&2
  echo "GDB exit code: ${gdb_exit_code}" >&2
  echo "--- gdb output ---" >&2
  cat "${GDB_LOG}" >&2
  echo "--- qemu serial output ---" >&2
  cat "${SERIAL_LOG}" >&2
  exit 1
fi

echo "PASS: arm64 semihosting through GDB"