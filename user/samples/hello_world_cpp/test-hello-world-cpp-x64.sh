#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"

"${repo_root}/tests/build-tests.sh" \
  "${script_dir}" \
  x64 \
  hello_world_cpp_x64 \
  hello_world_cpp \
  x64 \
  ringos_x64_hello_world_cpp \
  "[gdb] hello world from libc++"
