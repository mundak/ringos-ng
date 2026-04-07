#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"

"${repo_root}/tests/build-tests.sh" \
  "${script_dir}" \
  arm64 \
  hello_world_cpp_arm64 \
  hello_world_cpp \
  arm64 \
  ringos_arm64_hello_world_cpp \
  "[gdb] hello world from libc++"
