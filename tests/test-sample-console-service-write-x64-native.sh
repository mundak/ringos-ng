#!/usr/bin/env bash
# Smoke test for the x64 kernel image that boots the console_service_write sample.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-ringos_x64_console_service_write>" >&2
  exit 1
fi

KERNEL_IMAGE="$1"
TIMEOUT_SECONDS=15

DEBUG_LOG="$(mktemp)"
trap 'rm -f "${DEBUG_LOG}"' EXIT

timeout "${TIMEOUT_SECONDS}" \
  env RINGOS_DEBUGCON="file:${DEBUG_LOG}" \
  "$(dirname "${BASH_SOURCE[0]}")/../scripts/run-x64.sh" "${KERNEL_IMAGE}" >/dev/null 2>&1 \
  || true

for expected_line in \
  "[gdb] ringos x64" \
  "[gdb] gdb hooks ready" \
  "[gdb] hello world" \
  "[gdb] x64 initial user runtime ready" \
  "hello from console service sample"; do
  if ! grep -Fq -- "${expected_line}" "${DEBUG_LOG}"; then
    echo "FAIL: expected '${expected_line}' not found in x64 console_service_write debug output" >&2
    echo "--- debug output ---" >&2
    cat "${DEBUG_LOG}" >&2
    exit 1
  fi
done

echo "PASS: x64 console_service_write sample test"
