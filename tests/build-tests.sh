#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 6 || $# -gt 7 ]]; then
  echo "Usage: tests/build-tests.sh <sample-dir> <sample-target-arch> <build-name> <sample-cmake-target> <kernel-target-arch> <kernel-target> [expected-output-line]" >&2
  exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
sample_dir_input="$1"
sample_target_arch="$2"
build_name="$3"
cmake_target="$4"
kernel_target_arch="$5"
kernel_target="$6"
expected_output_line="${7:-[gdb] hello world from ANSI C}"
release_repo="${GITHUB_REPOSITORY:-mundak/ringos-ng}"

case "${sample_target_arch}" in
  x64|arm64)
    ;;
  *)
    echo "Unsupported sample target architecture: ${sample_target_arch}" >&2
    exit 1
    ;;
esac

case "${kernel_target_arch}" in
  x64|arm64)
    ;;
  *)
    echo "Unsupported kernel target architecture: ${kernel_target_arch}" >&2
    exit 1
    ;;
esac

if [[ "${sample_dir_input}" = /* ]]; then
  sample_dir="${sample_dir_input}"
else
  sample_dir="${repo_root}/${sample_dir_input}"
fi

if [[ ! -f "${sample_dir}/CMakeLists.txt" ]]; then
  echo "Sample directory does not contain CMakeLists.txt: ${sample_dir}" >&2
  exit 1
fi

sample_build_root="${repo_root}/build/sample-tests/${build_name}"
kernel_build_root="${repo_root}/build/sample-tests/${build_name}_kernel"
toolchain_root="${repo_root}/build/toolchain"
toolchain_file="${toolchain_root}/cmake/ringos-toolchain.cmake"
kernel_toolchain_file="${repo_root}/kernel/toolchains/${kernel_target_arch}.cmake"

rm -rf "${sample_build_root}" "${kernel_build_root}"

"${repo_root}/tests/download-latest-toolchain.sh" \
  --repo "${release_repo}" \
  --archive-dir "${repo_root}/build" \
  --install-root "${toolchain_root}"

if [[ ! -f "${toolchain_file}" ]]; then
  echo "Installed toolchain file not found: ${toolchain_file}" >&2
  exit 1
fi

if [[ ! -f "${kernel_toolchain_file}" ]]; then
  echo "Kernel toolchain file not found: ${kernel_toolchain_file}" >&2
  exit 1
fi

cmake -S "${sample_dir}" \
  -B "${sample_build_root}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
  -DRINGOS_TARGET_ARCH="${sample_target_arch}"

cmake --build "${sample_build_root}" --target "${cmake_target}"

sample_executable="${sample_build_root}/${cmake_target}.exe"

if [[ ! -f "${sample_executable}" ]]; then
  echo "Expected executable was not produced: ${sample_executable}" >&2
  exit 1
fi

case "${kernel_target}" in
  ringos_x64)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_HELLO_WORLD_X64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos x64"
    expected_runtime_ready_line="[gdb] x64 initial user runtime ready"
    ;;
  ringos_x64_hello_world_cpp)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos x64"
    expected_runtime_ready_line="[gdb] x64 initial user runtime ready"
    ;;
  ringos_arm64)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_HELLO_WORLD_ARM64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos arm64"
    expected_runtime_ready_line="[gdb] arm64 initial user runtime ready"
    ;;
  ringos_arm64_hello_world_cpp)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_HELLO_WORLD_CPP_ARM64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos arm64"
    expected_runtime_ready_line="[gdb] arm64 initial user runtime ready"
    ;;
  ringos_arm64_x64_emulator)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_HELLO_WORLD_X64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos arm64"
    expected_runtime_ready_line="[gdb] arm64 x64 emulator runtime ready"
    ;;
  ringos_arm64_x64_emulator_hello_world_cpp)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_HELLO_WORLD_CPP_X64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos arm64"
    expected_runtime_ready_line="[gdb] arm64 x64 emulator runtime ready"
    ;;
  ringos_x64_console_service_write)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_CONSOLE_SERVICE_WRITE_X64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos x64"
    expected_runtime_ready_line="[gdb] x64 initial user runtime ready"
    ;;
  ringos_arm64_console_service_write)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_CONSOLE_SERVICE_WRITE_ARM64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos arm64"
    expected_runtime_ready_line="[gdb] arm64 initial user runtime ready"
    ;;
  ringos_arm64_x64_emulator_console_service_write)
    kernel_test_app_cmake_arg="-DRINGOS_TEST_APP_CONSOLE_SERVICE_WRITE_X64_BINARY=${sample_executable}"
    expected_kernel_banner_line="[gdb] ringos arm64"
    expected_runtime_ready_line="[gdb] arm64 x64 emulator runtime ready"
    ;;
  *)
    echo "Unsupported kernel target: ${kernel_target}" >&2
    exit 1
    ;;
esac

cmake -S "${repo_root}" \
  -B "${kernel_build_root}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${kernel_toolchain_file}" \
  -DRINGOS_TOOLCHAIN_ROOT="${toolchain_root}" \
  -DRINGOS_TARGET_ARCH="${kernel_target_arch}" \
  -DRINGOS_ENABLE_TESTING=OFF \
  "${kernel_test_app_cmake_arg}"

cmake --build "${kernel_build_root}" --target "${kernel_target}"

kernel_image="${kernel_build_root}/arch/${kernel_target_arch}/${kernel_target}"

if [[ ! -f "${kernel_image}" ]]; then
  echo "Expected kernel image was not produced: ${kernel_image}" >&2
  exit 1
fi

debug_log="$(mktemp)"
qemu_pid=""
log_stream_pid=""
timeout_seconds="${RINGOS_QEMU_TIMEOUT_SECONDS:-15}"

cleanup()
{
  if [[ -n "${log_stream_pid}" ]]; then
    kill "${log_stream_pid}" 2>/dev/null || true
    wait "${log_stream_pid}" 2>/dev/null || true
  fi

  if [[ -n "${qemu_pid}" ]]; then
    kill "${qemu_pid}" 2>/dev/null || true
    wait "${qemu_pid}" 2>/dev/null || true
  fi

  rm -f "${debug_log}"
}

launch_qemu()
{
  if [[ "${kernel_target_arch}" == "x64" ]]; then
    local qemu_bin="${RINGOS_QEMU_BIN:-qemu-system-x86_64}"

    "${qemu_bin}" \
      -kernel "${kernel_image}" \
      -display none \
      -debugcon "file:${debug_log}" \
      -global isa-debugcon.iobase=0xe9 \
      -monitor none \
      -no-reboot >>"${debug_log}" 2>&1 &
  else
    local qemu_bin="${RINGOS_QEMU_BIN:-qemu-system-aarch64}"

    "${qemu_bin}" \
      -machine virt \
      -cpu cortex-a57 \
      -kernel "${kernel_image}" \
      -display none \
      -semihosting-config enable=on,target=native \
      -no-reboot >"${debug_log}" 2>&1 &
  fi

  qemu_pid="$!"
}

all_expected_lines_present()
{
  local expected_line

  for expected_line in \
    "${expected_kernel_banner_line}" \
    "[gdb] gdb hooks ready" \
    "[gdb] hello world" \
    "${expected_runtime_ready_line}" \
    "${expected_output_line}"; do
    if ! grep -Fq -- "${expected_line}" "${debug_log}"; then
      return 1
    fi
  done

  return 0
}

trap cleanup EXIT
launch_qemu
tail -n +1 -f "${debug_log}" &
log_stream_pid="$!"

deadline="$((SECONDS + timeout_seconds))"
test_passed=0
qemu_exit_status=""

while (( SECONDS < deadline )); do
  if all_expected_lines_present; then
    test_passed=1
    break
  fi

  if ! kill -0 "${qemu_pid}" 2>/dev/null; then
    set +e
    wait "${qemu_pid}"
    qemu_exit_status="$?"
    set -e
    qemu_pid=""
    break
  fi

  sleep 1
done

if [[ "${test_passed}" == "1" ]]; then
  echo "Built ${cmake_target}.exe for ${sample_target_arch} using the published toolchain bundle"
  echo "Sample build directory: ${sample_build_root}"
  echo "Sample executable: ${sample_executable}"
  echo "Kernel build directory: ${kernel_build_root}"
  echo "Kernel image: ${kernel_image}"
  echo "PASS: ${kernel_target} booted the compiled ${cmake_target}.exe under QEMU"
  exit 0
fi

if [[ -n "${qemu_pid}" ]]; then
  echo "FAIL: timed out waiting for expected QEMU output from ${kernel_target}" >&2
else
  echo "FAIL: QEMU exited before expected output from ${kernel_target} (status ${qemu_exit_status})" >&2
fi
exit 1
