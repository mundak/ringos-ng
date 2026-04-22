#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: user/samples/console_service_write/test-console-service-write.sh <x64-native|arm64-native|arm64-x64-emulator>" >&2
  exit 1
fi

if [[ "$1" == "arm64-x64-emulator" ]]; then
  echo "console_service_write does not support arm64-x64-emulator yet: the arm64 x64 emulator runtime cannot bootstrap srv_terminal." >&2
  exit 1
fi

if [[ "$1" != "x64-native" && "$1" != "arm64-native" ]]; then
  echo "Unsupported console_service_write lane: $1" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"

"${repo_root}/tests/build-tests.sh" "${script_dir}" "$1" "[gdb] hello world through srv_terminal"
