#!/usr/bin/env bash
# Build the shared ringos toolchain package for all targets and archive it as a distributable tar.xz.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

need_tool()
{
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool not found: $1" >&2
    exit 1
  fi
}

log_step()
{
  printf '[toolchain][%s] %s\n' "$(date -u +%H:%M:%S)" "$*"
}

resolve_job_count()
{
  local override="${1:-}"
  local kib_per_job="${2:-0}"
  local cpu_count=""
  local available_kib=""
  local memory_limited_jobs=0
  local jobs=1

  if [[ -n "${override}" ]]; then
    if ! [[ "${override}" =~ ^[0-9]+$ ]] || [[ "${override}" -lt 1 ]]; then
      echo "Expected a positive integer job count, got '${override}'." >&2
      exit 1
    fi

    printf '%s\n' "${override}"
    return
  fi

  cpu_count="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  if [[ -z "${cpu_count}" ]]; then
    cpu_count="$(nproc 2>/dev/null || true)"
  fi

  if [[ "${cpu_count}" =~ ^[0-9]+$ ]] && [[ "${cpu_count}" -gt 0 ]]; then
    jobs="${cpu_count}"
  fi

  if [[ "${kib_per_job}" =~ ^[0-9]+$ ]] && [[ "${kib_per_job}" -gt 0 ]] && [[ -r /proc/meminfo ]]; then
    available_kib="$({
      awk '/MemAvailable:/ { print $2; found = 1; exit }
           /MemTotal:/ { if (!found) total = $2 }
           END { if (!found && total) print total }' /proc/meminfo
    } 2>/dev/null || true)"

    if [[ "${available_kib}" =~ ^[0-9]+$ ]] && [[ "${available_kib}" -gt 0 ]]; then
      memory_limited_jobs=$((available_kib / kib_per_job))

      if [[ "${memory_limited_jobs}" -lt 1 ]]; then
        memory_limited_jobs=1
      fi

      if [[ "${memory_limited_jobs}" -lt "${jobs}" ]]; then
        jobs="${memory_limited_jobs}"
      fi
    fi
  fi

  printf '%s\n' "${jobs}"
}

run_with_heartbeat()
{
  local description="$1"
  local heartbeat_pid=""
  local interval="${RINGOS_TOOLCHAIN_HEARTBEAT_SECONDS:-60}"
  shift

  log_step "${description}"

  if [[ "${interval}" =~ ^[0-9]+$ ]] && [[ "${interval}" -gt 0 ]]; then
    (
      while true; do
        sleep "${interval}"
        log_step "${description} still running..."
      done
    ) &
    heartbeat_pid=$!
  fi

  set +e
  (
    set -e
    "$@"
  )
  local status=$?
  set -e

  if [[ -n "${heartbeat_pid}" ]]; then
    kill "${heartbeat_pid}" >/dev/null 2>&1 || true
    wait "${heartbeat_pid}" 2>/dev/null || true
  fi

  if [[ "${status}" -ne 0 ]]; then
    log_step "${description} failed."
    return "${status}"
  fi

  log_step "${description} completed."
}

cleanup_llvm_bootstrap()
{
  if [[ -n "${normalized_patch_dir:-}" && -d "${normalized_patch_dir}" ]]; then
    rm -rf "${normalized_patch_dir}"
  fi

  if [[ -n "${extract_root:-}" && -d "${extract_root}" ]]; then
    rm -rf "${extract_root}"
  fi
}

archive_stem_from_path()
{
  local archive_path="$1"
  local archive_name=""

  archive_name="$(basename "${archive_path}")"

  case "${archive_name}" in
    *.tar.gz)
      printf '%s\n' "${archive_name%.tar.gz}"
      ;;
    *.tar.xz)
      printf '%s\n' "${archive_name%.tar.xz}"
      ;;
    *.tgz)
      printf '%s\n' "${archive_name%.tgz}"
      ;;
    *.zip)
      printf '%s\n' "${archive_name%.zip}"
      ;;
    *)
      printf '%s\n' "${archive_name%.*}"
      ;;
  esac
}

prepare_llvm_source_tree()
{
  local archive_parent_dir=""
  local archive_tmp=""
  local first_archive_entry=""
  local extracted_root_name=""
  local extracted_source_dir=""

  archive_parent_dir="$(dirname "${llvm_source_dir}")"
  mkdir -p "${archive_parent_dir}" "${llvm_download_dir}"

  if [[ ! -f "${llvm_source_archive}" ]]; then
    log_step "Downloading pinned LLVM source archive ${llvm_ref} into ${llvm_source_archive}"
    archive_tmp="${llvm_source_archive}.tmp"
    rm -f "${archive_tmp}"
    curl --fail --location --retry 3 --silent --show-error \
      --output "${archive_tmp}" \
      "${llvm_source_archive_url}"
    mv "${archive_tmp}" "${llvm_source_archive}"
  else
    log_step "Using cached LLVM source archive ${llvm_source_archive}"
  fi

  extract_root="$(mktemp -d)"
  first_archive_entry="$(tar -tzf "${llvm_source_archive}" | awk 'NR == 1 { print; found = 1 } END { if (!found) exit 1 }')"
  extracted_root_name="${first_archive_entry%%/*}"

  if [[ -z "${extracted_root_name}" ]]; then
    echo "Unable to determine the top-level directory in ${llvm_source_archive}." >&2
    exit 1
  fi

  log_step "Extracting LLVM source tree into ${llvm_source_dir}"
  rm -rf "${llvm_source_dir}"
  tar -xzf "${llvm_source_archive}" -C "${extract_root}"

  extracted_source_dir="${extract_root}/${extracted_root_name}"
  if [[ ! -d "${extracted_source_dir}" ]]; then
    echo "Expected extracted LLVM source directory ${extracted_source_dir} is missing." >&2
    exit 1
  fi

  mkdir -p "${archive_parent_dir}"
  mv "${extracted_source_dir}" "${llvm_source_dir}"
  log_step "Prepared extracted LLVM source tree at ${llvm_source_dir}"

  if [[ ! -d "${llvm_source_dir}/.git" ]]; then
    log_step "Initializing temporary git metadata in extracted LLVM source tree"
    git -C "${llvm_source_dir}" init -q
  fi
}

apply_llvm_patches()
{
  local normalized_patch_file=""
  local patch_file=""
  local -a patch_files=()

  if [[ ! -d "${llvm_patch_dir}" ]]; then
    log_step "No LLVM patch directory found under ${llvm_patch_dir}; using upstream source as-is"
    return 0
  fi

  shopt -s nullglob
  patch_files=("${llvm_patch_dir}"/*.patch)
  shopt -u nullglob

  if [[ "${#patch_files[@]}" -eq 0 ]]; then
    log_step "No LLVM patches found under ${llvm_patch_dir}"
    return 0
  fi

  log_step "Applying ${#patch_files[@]} LLVM patch(es) from ${llvm_patch_dir}"
  normalized_patch_dir="$(mktemp -d)"

  for patch_file in "${patch_files[@]}"; do
    normalized_patch_file="${normalized_patch_dir}/$(basename "${patch_file}")"
    sed 's/\r$//' "${patch_file}" > "${normalized_patch_file}"

    if git -C "${llvm_source_dir}" apply --check "${normalized_patch_file}"; then
      git -C "${llvm_source_dir}" apply "${normalized_patch_file}"
    else
      if git -C "${llvm_source_dir}" apply --reverse --check "${normalized_patch_file}"; then
        echo "Skipping already-applied patch ${patch_file}"
      else
        echo "Failed to apply patch ${patch_file}" >&2
        exit 1
      fi
    fi
  done
}

build_bootstrap_llvm()
{
  mkdir -p "${llvm_build_dir}" "${llvm_install_dir}"

  run_with_heartbeat "Configuring bootstrap LLVM build in ${llvm_build_dir}" \
    cmake -S "${llvm_source_dir}/llvm" -B "${llvm_build_dir}" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${llvm_install_dir}" \
      -DLLVM_ENABLE_PROJECTS="${llvm_projects}" \
      -DLLVM_ENABLE_RUNTIMES="${llvm_runtimes}" \
      -DLLVM_TARGETS_TO_BUILD="${llvm_targets}" \
      -DLLVM_PARALLEL_COMPILE_JOBS="${bootstrap_compile_jobs}" \
      -DLLVM_PARALLEL_LINK_JOBS="${bootstrap_link_jobs}" \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_INCLUDE_TESTS=OFF

  run_with_heartbeat "Building and installing bootstrap LLVM toolchain into ${llvm_install_dir}" \
    cmake --build "${llvm_build_dir}" --target install --parallel "${bootstrap_compile_jobs}"
}

usage()
{
  cat <<EOF
Usage: tools/toolchain/build-toolchain.sh [options] [output-archive]

Options:
  --output <path>          Output archive path.
  --version <version>      Toolchain version recorded in the bundle metadata.
  --help                   Show this help text.
EOF
}

output_archive=""
toolchain_version="${RINGOS_TOOLCHAIN_VERSION:-}"
normalized_patch_dir=""
extract_root=""
skip_bootstrap="${RINGOS_TOOLCHAIN_SKIP_BOOTSTRAP:-0}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output)
      output_archive="$2"
      shift 2
      ;;
    --version)
      toolchain_version="$2"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      if [[ -z "${output_archive}" ]]; then
        output_archive="$1"
        shift
      else
        echo "Unknown argument: $1" >&2
        usage >&2
        exit 1
      fi
      ;;
  esac
done

if [[ -z "${toolchain_version}" ]]; then
  toolchain_version="dev-local"
fi

log_step "Preparing shared toolchain build for version ${toolchain_version}"

install_root="${repo_root}/build/toolchain"
toolchain_build_root="${repo_root}/build/toolchain-build"
build_llvm_root="${toolchain_build_root}/bootstrap-llvm"
bootstrap_toolchain_root="${build_llvm_root}/install"
staging_root="${toolchain_build_root}/package"
package_root="${staging_root}/ringos-toolchain"
x64_build_dir="${toolchain_build_root}/x64"
arm64_build_dir="${toolchain_build_root}/arm64"

mkdir -p "${repo_root}/build" "${toolchain_build_root}" "${build_llvm_root}"
rm -rf "${install_root}" "${staging_root}"
mkdir -p "${install_root}" "${staging_root}"

# Recreate the payload configure trees so cached CMakeSystem.cmake files do not
# hold on to deleted toolchain-file paths from earlier layout revisions.
rm -rf "${x64_build_dir}" "${arm64_build_dir}"

llvm_repo_url="${RINGOS_LLVM_REPO_URL:-https://github.com/llvm/llvm-project.git}"
llvm_ref="${RINGOS_LLVM_REF:-3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff}"
llvm_repo_archive_base="${llvm_repo_url%.git}"
llvm_source_archive_url="${RINGOS_LLVM_SOURCE_ARCHIVE_URL:-${llvm_repo_archive_base}/archive/${llvm_ref}.tar.gz}"
llvm_download_dir="${RINGOS_LLVM_DOWNLOAD_DIR:-${build_llvm_root}/downloads}"
llvm_source_archive="${RINGOS_LLVM_SOURCE_ARCHIVE:-${llvm_download_dir}/llvm-project-${llvm_ref}.tar.gz}"
llvm_source_dir_name="$(archive_stem_from_path "${llvm_source_archive}")"
llvm_root="${RINGOS_LLVM_ROOT:-${repo_root}/tools/llvm}"
llvm_source_dir="${build_llvm_root}/src/${llvm_source_dir_name}"
llvm_build_dir="${RINGOS_LLVM_BUILD_DIR:-${build_llvm_root}/build-${llvm_ref}}"
llvm_install_dir="${RINGOS_LLVM_INSTALL_DIR:-${bootstrap_toolchain_root}}"
llvm_patch_dir="${RINGOS_LLVM_PATCH_DIR:-${llvm_root}/patches}"
llvm_projects="${RINGOS_LLVM_ENABLE_PROJECTS:-clang;lld}"
llvm_runtimes="${RINGOS_LLVM_ENABLE_RUNTIMES:-compiler-rt}"
llvm_targets="${RINGOS_LLVM_TARGETS_TO_BUILD:-AArch64;X86}"
bootstrap_compile_jobs="$(resolve_job_count "${RINGOS_TOOLCHAIN_BOOTSTRAP_JOBS:-}" 3145728)"
bootstrap_link_jobs="$(resolve_job_count "${RINGOS_TOOLCHAIN_BOOTSTRAP_LINK_JOBS:-}" 8388608)"
payload_build_jobs="$(resolve_job_count "${RINGOS_TOOLCHAIN_PAYLOAD_JOBS:-${bootstrap_compile_jobs}}" 0)"

need_tool curl
need_tool git
need_tool tar
need_tool cmake

mkdir -p "${llvm_root}" "$(dirname "${llvm_source_dir}")" "$(dirname "${llvm_install_dir}")"

trap cleanup_llvm_bootstrap EXIT

if [[ -z "${llvm_source_dir_name}" ]]; then
  echo "Unable to derive an LLVM source directory name from ${llvm_source_archive}." >&2
  exit 1
fi

if [[ "${skip_bootstrap}" == "1" ]]; then
  if [[ ! -x "${llvm_install_dir}/bin/clang" ]]; then
    echo "Cannot skip bootstrap LLVM build because ${llvm_install_dir}/bin/clang is missing." >&2
    exit 1
  fi

  log_step "Skipping bootstrap LLVM rebuild and reusing ${llvm_install_dir}"
else
  if [[ -d "${llvm_build_dir}" ]]; then
    log_step "Reusing bootstrap build tree at ${llvm_build_dir}"
  elif [[ -d "${llvm_source_dir}" ]]; then
    log_step "Reusing prepared LLVM source tree at ${llvm_source_dir}"
  else
    log_step "Preparing pinned LLVM source tree"
    prepare_llvm_source_tree
    log_step "Preparing pinned LLVM source tree completed."

    log_step "Applying RingOS LLVM patch series"
    apply_llvm_patches
    log_step "Applying RingOS LLVM patch series completed."
  fi

  log_step "Using ${bootstrap_compile_jobs} bootstrap compile job(s), ${bootstrap_link_jobs} bootstrap link job(s), and ${payload_build_jobs} payload build job(s)"
  build_bootstrap_llvm
fi

validate_packaged_toolchain()
{
  local bundle_root="$1"
  local -a expected_tools=(clang clang++ lld-link llvm-ar llvm-ranlib llvm-objcopy)
  local -a expected_target_triples=(x86_64-unknown-ringos-msvc aarch64-unknown-ringos-msvc)
  local tool_name=""
  local target_triple=""

  for tool_name in "${expected_tools[@]}"; do
    if [[ ! -e "${bundle_root}/bin/${tool_name}" ]]; then
      echo "Packaged toolchain bundle is missing bin/${tool_name}." >&2
      exit 1
    fi
  done

  for target_triple in "${expected_target_triples[@]}"; do
    if [[ ! -e "${bundle_root}/sysroots/${target_triple}/include/c++/v1/__config" ]]; then
      echo "Packaged toolchain bundle is missing libc++ headers for ${target_triple}." >&2
      exit 1
    fi
  done
}

run_with_heartbeat "Configuring x64 installed-toolchain build in ${x64_build_dir}" \
  cmake -S "${repo_root}/tools/toolchain" \
    -B "${x64_build_dir}" \
    -G Ninja \
    -DRINGOS_ACTIVE_LLVM_ROOT="${llvm_install_dir}" \
    -DRINGOS_TARGET_ARCH=x64 \
    -DRINGOS_TOOLCHAIN_VERSION="${toolchain_version}" \
    -DRINGOS_TOOLCHAIN_ROOT="${install_root}"
run_with_heartbeat "Building x64 installed-toolchain payload" \
  cmake --build "${x64_build_dir}" --target ringos_installed_toolchain --parallel "${payload_build_jobs}"

run_with_heartbeat "Configuring arm64 installed-toolchain build in ${arm64_build_dir}" \
  cmake -S "${repo_root}/tools/toolchain" \
    -B "${arm64_build_dir}" \
    -G Ninja \
    -DRINGOS_ACTIVE_LLVM_ROOT="${llvm_install_dir}" \
    -DRINGOS_TARGET_ARCH=arm64 \
    -DRINGOS_TOOLCHAIN_VERSION="${toolchain_version}" \
    -DRINGOS_TOOLCHAIN_ROOT="${install_root}"
run_with_heartbeat "Building arm64 installed-toolchain payload" \
  cmake --build "${arm64_build_dir}" --target ringos_installed_toolchain --parallel "${payload_build_jobs}"

toolchain_version_file="${install_root}/share/ringos/toolchain-version.txt"
printf '%s\n' "${toolchain_version}" > "${toolchain_version_file}"

archive_stem="ringos-toolchain-${toolchain_version}"

if [[ -z "${output_archive}" ]]; then
  output_archive="${repo_root}/build/${archive_stem}.tar.xz"
fi

case "${output_archive}" in
  /*)
    output_archive_path="${output_archive}"
    ;;
  *)
    output_archive_path="$PWD/${output_archive}"
    ;;
esac

mkdir -p "$(dirname "${output_archive_path}")"

log_step "Staging packaged toolchain bundle under ${package_root}"
mkdir -p "${package_root}"
cp -a "${install_root}/." "${package_root}/"
run_with_heartbeat "Validating packaged toolchain bundle" validate_packaged_toolchain "${package_root}"

rm -f "${output_archive_path}"
log_step "Writing versioned toolchain archive to ${output_archive_path}"
(
  cd "${staging_root}"
  cmake -E tar cJf "${output_archive_path}" ringos-toolchain
)

echo "Built shared toolchain archive: ${output_archive_path}"
echo "Toolchain version: ${toolchain_version}"
echo "x64 toolchain file in archive: ringos-toolchain/cmake/ringos-x64-toolchain.cmake"
echo "arm64 toolchain file in archive: ringos-toolchain/cmake/ringos-arm64-toolchain.cmake"
echo "generic toolchain file in archive: ringos-toolchain/cmake/ringos-toolchain.cmake"
