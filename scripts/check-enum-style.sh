#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

search() {
  if command -v rg >/dev/null 2>&1; then
    rg -n --glob '!build/**' "$@"
    return
  fi

  local pattern="$1"
  shift
  grep -RIn --exclude-dir=build --include='*.h' --include='*.hpp' --include='*.c' --include='*.cpp' -- "$pattern" "$@"
}

enum_class_matches="$(search 'enum class' arch emulator kernel user win32 || true)"

if [[ -n "${enum_class_matches}" ]]; then
  echo "error: enum class is forbidden; use plain enums with explicit underlying types in C++."
  echo "${enum_class_matches}"
  exit 1
fi

sdk_define_matches="$({
  search '^#define RINGOS_CONSOLE_' user/sdk/include/ringos/console.h || true
  search '^#define RINGOS_STATUS_' user/sdk/include/ringos/status.h || true
  search '^#define RINGOS_SYSCALL_' user/sdk/include/ringos/syscalls.h || true
  search '^#define RINGOS_INVALID_HANDLE' user/sdk/include/ringos/handle.h || true
} | sed '/^$/d' || true)"

if [[ -n "${sdk_define_matches}" ]]; then
  echo "error: SDK numeric constants must use enum-backed declarations instead of #define."
  echo "${sdk_define_matches}"
  exit 1
fi

echo "PASS: enum style checks"