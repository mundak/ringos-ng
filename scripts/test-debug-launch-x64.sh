#!/usr/bin/env bash
# Deterministic test for the x64 GDB launch wrapper.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIRECTORY="$(mktemp -d)"
FAKE_QEMU="${TEST_DIRECTORY}/fake-qemu-x64.sh"
ARGUMENT_LOG="${TEST_DIRECTORY}/arguments.log"
KERNEL_IMAGE="${TEST_DIRECTORY}/ringos_x64"

cleanup()
{
  rm -rf "${TEST_DIRECTORY}"
}

trap cleanup EXIT

cat <<'EOF' > "${FAKE_QEMU}"
#!/usr/bin/env bash
set -euo pipefail

printf '%s\n' "$@" > "${RINGOS_TEST_ARGUMENT_LOG}"
EOF

chmod +x "${FAKE_QEMU}"
touch "${KERNEL_IMAGE}"

RINGOS_TEST_ARGUMENT_LOG="${ARGUMENT_LOG}" \
RINGOS_QEMU_BIN="${FAKE_QEMU}" \
RINGOS_GDB_PORT=4321 \
RINGOS_DEBUGCON="file:/tmp/ringos-debugcon.log" \
  "${SCRIPT_DIR}/debug-x64.sh" "${KERNEL_IMAGE}"

for expected_argument in \
  -kernel \
  "${KERNEL_IMAGE}" \
  -display \
  none \
  -debugcon \
  file:/tmp/ringos-debugcon.log \
  -global \
  isa-debugcon.iobase=0xe9 \
  -monitor \
  none \
  -no-reboot \
  -gdb \
  tcp::4321 \
  -S; do
  if ! grep -Fxq -- "${expected_argument}" "${ARGUMENT_LOG}"; then
    echo "FAIL: expected argument '${expected_argument}' was not passed to QEMU" >&2
    echo "--- captured arguments ---" >&2
    cat "${ARGUMENT_LOG}" >&2
    exit 1
  fi
done

echo "PASS: x64 debug launch wrapper"
