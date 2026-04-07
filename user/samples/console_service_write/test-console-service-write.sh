#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
target_lane="${1:-}"

if [[ -z "${target_lane}" ]]; then
  echo "Usage: ${0##*/} <x64|arm64|x64-on-arm64>" >&2
  exit 1
fi

case "${target_lane}" in
  x64)
    sample_target_arch="x64"
    build_name="console_service_write_x64"
    kernel_target_arch="x64"
    kernel_target="ringos_x64_console_service_write"
    ;;
  arm64)
    sample_target_arch="arm64"
    build_name="console_service_write_arm64"
    kernel_target_arch="arm64"
    kernel_target="ringos_arm64_console_service_write"
    ;;
  x64-on-arm64)
    sample_target_arch="x64"
    build_name="console_service_write_x64_on_arm64"
    kernel_target_arch="arm64"
    kernel_target="ringos_arm64_x64_emulator_console_service_write"
    ;;
  *)
    echo "Unsupported target lane: ${target_lane}" >&2
    exit 1
    ;;
esac

"${repo_root}/tests/build-tests.sh" \
  "${script_dir}" \
  "${sample_target_arch}" \
  "${build_name}" \
  console_service_write \
  "${kernel_target_arch}" \
  "${kernel_target}" \
  "hello from console service sample"
