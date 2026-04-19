#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: user/samples/hello_world/test-hello-world.sh <x64-native|arm64-native|arm64-x64-emulator>" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"

"${repo_root}/tests/build-tests.sh" "${script_dir}" "$1"
