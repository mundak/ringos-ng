#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"

"${repo_root}/tests/build-tests.sh" \
  "${script_dir}" \
  x64 \
  hello_world_x64_on_arm64 \
  hello_world \
  arm64 \
  ringos_arm64_x64_emulator
