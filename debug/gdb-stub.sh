#!/usr/bin/env bash

set -euo pipefail

usage()
{
  cat <<'EOF'
Usage: gdb-stub.sh --arch <x64|arm64> [options]

Options:
  --arch <arch>                 Target architecture to debug.
  --sample-project <path>       Sample project path inside the copied repo.
                                Default: /repo/user/samples/hello_world
  --sample-target <target>      Sample target name. Default: hello_world
  --kernel-target <target>      Kernel target name. Default: ringos_<arch>
  --test-app-binary-var <name>  CMake cache variable used to embed the built
                                sample into the kernel target. When omitted,
                                the kernel target's default embedded app is used.
  --breakpoint <symbol>         Breakpoint symbol to trace.
  --hit-count <count>           Stop after this many breakpoint hits.
                                Default: 8
  --gdb-port <port>             QEMU GDB stub port. Default: RINGOS_GDB_PORT
                                or 1234
  --toolchain-archive-dir <p>   Directory that contains ringos-toolchain-*.tar.xz.
                                Default: /workspace-host/build
  --help                        Show this help text.

The defaults are tuned for the hello_world sample on x64 and arm64.
EOF
}

target_arch=""
sample_project="/repo/user/samples/hello_world"
sample_target="hello_world"
kernel_target=""
test_app_binary_var=""
breakpoint_symbol=""
hit_count="8"
gdb_port="${RINGOS_GDB_PORT:-1234}"
toolchain_archive_dir="/workspace-host/build"

while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --arch)
      target_arch="$2"
      shift 2
      ;;
    --sample-project)
      sample_project="$2"
      shift 2
      ;;
    --sample-target)
      sample_target="$2"
      shift 2
      ;;
    --kernel-target)
      kernel_target="$2"
      shift 2
      ;;
    --test-app-binary-var)
      test_app_binary_var="$2"
      shift 2
      ;;
    --breakpoint)
      breakpoint_symbol="$2"
      shift 2
      ;;
    --hit-count)
      hit_count="$2"
      shift 2
      ;;
    --gdb-port)
      gdb_port="$2"
      shift 2
      ;;
    --toolchain-archive-dir)
      toolchain_archive_dir="$2"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -z "${target_arch}" ]]; then
  echo "--arch is required" >&2
  usage >&2
  exit 1
fi

case "${target_arch}" in
  x64)
    kernel_toolchain_file="/repo/kernel/toolchains/x64.cmake"
    qemu_bin_default="qemu-system-x86_64"
    kernel_target="${kernel_target:-ringos_x64}"
    breakpoint_symbol="${breakpoint_symbol:-x64_handle_syscall}"
    test_app_binary_var="${test_app_binary_var:-RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY}"
    kernel_output_dir="/work/kernel-${target_arch}/arch/x64"
    kernel_image_name="${kernel_target}"
    kernel_symbol_name="${kernel_target}.elf64"
    ;;
  arm64)
    kernel_toolchain_file="/repo/kernel/toolchains/arm64.cmake"
    qemu_bin_default="qemu-system-aarch64"
    kernel_target="${kernel_target:-ringos_arm64}"
    breakpoint_symbol="${breakpoint_symbol:-arm64_handle_syscall}"
    test_app_binary_var="${test_app_binary_var:-RINGOS_TEST_APP_HELLO_WORLD_ARM64_BINARY}"
    kernel_output_dir="/work/kernel-${target_arch}/arch/arm64"
    kernel_image_name="${kernel_target}"
    kernel_symbol_name="${kernel_target}"
    ;;
  *)
    echo "Unsupported architecture: ${target_arch}" >&2
    exit 1
    ;;
esac

sample_build_root="/work/sample-${sample_target}-${target_arch}"
kernel_build_root="/work/kernel-${target_arch}"
sample_executable="${sample_build_root}/${sample_target}.exe"
kernel_image="${kernel_output_dir}/${kernel_image_name}"
kernel_symbol_file="${kernel_output_dir}/${kernel_symbol_name}"
gdb_command_file="/tmp/ringos-debug-gdb.cmd"
gdb_output_file="/tmp/ringos-debug-gdb.out"
qemu_log_file="/tmp/ringos-debug-qemu.log"
qemu_stdout_file="/tmp/ringos-debug-qemu.stdout"

if [[ ! -d "${toolchain_archive_dir}" ]]; then
  echo "Toolchain archive directory does not exist: ${toolchain_archive_dir}" >&2
  exit 1
fi

if [[ -z "${test_app_binary_var}" ]]; then
  case "${target_arch}:${kernel_target}:${sample_project}:${sample_target}" in
    x64:ringos_x64:/repo/user/samples/hello_world:hello_world)
      test_app_binary_var="RINGOS_TEST_APP_HELLO_WORLD_X64_BINARY"
      ;;
    arm64:ringos_arm64:/repo/user/samples/hello_world:hello_world)
      test_app_binary_var="RINGOS_TEST_APP_HELLO_WORLD_ARM64_BINARY"
      ;;
  esac
fi

echo "[debug] preparing repo copy"

rm -rf /repo /work
mkdir -p /repo /work
(cd /workspace-host && tar cf - --exclude=build --exclude=.git .) | (cd /repo && tar xf -)
find /repo -type f -name '*.sh' -exec dos2unix {} + >/dev/null

cp "${toolchain_archive_dir}"/ringos-toolchain-*.tar.xz /work/

echo "[debug] extracting toolchain archive"
bash /repo/tests/download-latest-toolchain.sh \
  --archive-dir /work \
  --install-root /work/toolchain \
  --repo mundak/ringos-ng >/dev/null

if [[ -n "${test_app_binary_var}" ]]; then
  echo "[debug] building ${target_arch} sample"
  cmake -S "${sample_project}" \
    -B "${sample_build_root}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=/work/toolchain/cmake/ringos-toolchain.cmake \
    -DRINGOS_TARGET_ARCH="${target_arch}" >/dev/null
  cmake --build "${sample_build_root}" --target "${sample_target}" >/dev/null
else
  echo "[debug] using ${kernel_target}'s default embedded test app"
fi

echo "[debug] building ${target_arch} kernel"
kernel_cmake_args=(
  -S /repo
  -B "${kernel_build_root}"
  -G Ninja
  -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_TOOLCHAIN_FILE="${kernel_toolchain_file}"
  -DRINGOS_TOOLCHAIN_ROOT=/work/toolchain
  -DRINGOS_TARGET_ARCH="${target_arch}"
  -DRINGOS_ENABLE_TESTING=OFF
)

if [[ -n "${test_app_binary_var}" ]]; then
  kernel_cmake_args+=("-D${test_app_binary_var}=${sample_executable}")
fi

cmake "${kernel_cmake_args[@]}" >/dev/null
cmake --build "${kernel_build_root}" --target "${kernel_target}" >/dev/null

if [[ ! -f "${kernel_image}" ]]; then
  echo "Kernel image was not produced: ${kernel_image}" >&2
  exit 1
fi

if [[ ! -f "${kernel_symbol_file}" ]]; then
  echo "Kernel symbol file was not produced: ${kernel_symbol_file}" >&2
  exit 1
fi

echo "[debug] writing gdb command file"

if [[ "${target_arch}" == "x64" ]]; then
  cat >"${gdb_command_file}" <<EOF
set pagination off
set confirm off
file ${kernel_symbol_file}
target remote :${gdb_port}
set \$hit_count = 0
break ${breakpoint_symbol}
commands
silent
set \$hit_count = \$hit_count + 1
set \$frame = (x64_syscall_frame*)\$rdi
printf "hit=%llu syscall=%llu arg0=0x%llx arg1=0x%llx arg2=0x%llx arg3=0x%llx user_rsp=0x%llx\\n", \$hit_count, \$frame->rax, \$frame->rdi, \$frame->rsi, \$frame->rdx, \$frame->r10, \$frame->user_rsp
if \$hit_count >= ${hit_count}
  detach
  quit
end
continue
end
continue
EOF
else
  cat >"${gdb_command_file}" <<EOF
set pagination off
set confirm off
file ${kernel_symbol_file}
target remote :${gdb_port}
set \$hit_count = 0
break ${breakpoint_symbol}
commands
silent
set \$hit_count = \$hit_count + 1
set \$frame = (arm64_syscall_frame*)\$x0
printf "hit=%llu syscall=%llu arg0=0x%llx arg1=0x%llx arg2=0x%llx arg3=0x%llx user_sp=0x%llx esr=0x%llx\\n", \$hit_count, \$frame->x8, \$frame->x0, \$frame->x1, \$frame->x2, \$frame->x3, \$frame->sp_el0, \$frame->esr
if \$hit_count >= ${hit_count}
  detach
  quit
end
continue
end
continue
EOF
fi

cleanup()
{
  if [[ -n "${qemu_pid:-}" ]]; then
    kill "${qemu_pid}" 2>/dev/null || true
    wait "${qemu_pid}" 2>/dev/null || true
  fi
}

trap cleanup EXIT

echo "[debug] launching qemu"
qemu_bin="${RINGOS_QEMU_BIN:-${qemu_bin_default}}"

if [[ "${target_arch}" == "x64" ]]; then
  "${qemu_bin}" \
    -kernel "${kernel_image}" \
    -display none \
    -debugcon "file:${qemu_log_file}" \
    -global isa-debugcon.iobase=0xe9 \
    -monitor none \
    -no-reboot \
    -S \
    -gdb "tcp::${gdb_port}" >"${qemu_stdout_file}" 2>&1 &
else
  "${qemu_bin}" \
    -machine virt \
    -cpu cortex-a57 \
    -kernel "${kernel_image}" \
    -display none \
    -semihosting-config enable=on,target=native \
    -no-reboot \
    -S \
    -gdb "tcp::${gdb_port}" >"${qemu_log_file}" 2>&1 &
fi

qemu_pid="$!"
sleep 1

echo "[debug] running gdb batch session"
gdb-multiarch -q -batch -x "${gdb_command_file}" >"${gdb_output_file}" 2>&1 || true

echo "=== GDB ==="
sed -n '1,200p' "${gdb_output_file}"

if [[ -s "${qemu_log_file}" ]]; then
  echo "=== QEMU LOG ==="
  sed -n '1,120p' "${qemu_log_file}"
fi

if [[ -s "${qemu_stdout_file}" ]]; then
  echo "=== QEMU STDOUT ==="
  sed -n '1,120p' "${qemu_stdout_file}"
fi
