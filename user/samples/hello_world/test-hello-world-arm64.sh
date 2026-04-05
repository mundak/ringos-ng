#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"

"${repo_root}/tests/build-tests.sh" \
  "${script_dir}" \
  arm64 \
  hello_world_arm64 \
  hello_world
