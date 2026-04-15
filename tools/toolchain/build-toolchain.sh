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
  --repo <owner/name>      Repository used for release version resolution and publish.
  --output <path>          Output archive path.
  --output-dir <path>      Directory that should receive the output archive.
  --output-archive <path>  Explicit output archive path.
  --version <version>      Toolchain version recorded in the bundle metadata.
  --publish                Publish the built archive to GitHub Releases.
  --help                   Show this help text.
EOF
}

resolve_release_metadata()
{
  local release_repo="$1"
  local release_date=""
  local releases_json=""
  local sequence_number=""
  local curl_args=()

  if [[ -z "${release_repo}" ]]; then
    echo "Set --repo or GITHUB_REPOSITORY before resolving a toolchain release version." >&2
    exit 1
  fi

  need_tool curl
  need_tool python3
  need_tool date

  release_date="$(date -u +%Y.%m.%d)"
  releases_json="$(mktemp)"

  curl_args=(--fail --location --retry 3 --silent --show-error -H "Accept: application/vnd.github+json")

  if [[ -n "${github_token}" ]]; then
    curl_args+=(-H "Authorization: Bearer ${github_token}")
  fi

  curl "${curl_args[@]}" \
    --output "${releases_json}" \
    "https://api.github.com/repos/${release_repo}/releases?per_page=100"

  sequence_number="$(python3 - "${releases_json}" "${release_date}" <<'PY'
import json
import re
import sys

releases_path = sys.argv[1]
release_date = sys.argv[2]
pattern = re.compile(rf"^ringos-toolchain-{re.escape(release_date)}\.(\d+)$")
max_sequence = 0

with open(releases_path, "r", encoding="utf-8") as handle:
    releases = json.load(handle)

for release in releases:
    tag_name = release.get("tag_name", "")
    match = pattern.match(tag_name)
    if match is None:
        continue
    max_sequence = max(max_sequence, int(match.group(1)))

print(max_sequence + 1)
PY
  )"

  rm -f "${releases_json}"

  release_version="${release_date}.${sequence_number}"
  release_tag="ringos-toolchain-${release_version}"
  release_asset_name="${release_tag}.tar.xz"
}

resolve_clang_resource_dir()
{
  local clang_path="$1"
  local resource_dir=""

  resource_dir="$(${clang_path} --print-resource-dir)"

  if [[ -z "${resource_dir}" || ! -d "${resource_dir}" ]]; then
    echo "Unable to resolve clang resource directory from ${clang_path}." >&2
    exit 1
  fi

  printf '%s\n' "${resource_dir}"
}

write_arch_toolchain_file()
{
  local target_arch="$1"
  local target_triple="$2"
  local system_processor="$3"
  local output_file="$4"
  local clang_name="$5"
  local clangxx_name="$6"
  local llvm_ar_name="$7"
  local llvm_ranlib_name="$8"
  local llvm_objcopy_name="$9"
  local clang_resource_version="${10}"

  cat > "${output_file}" <<EOF
set(RINGOS_TARGET_ARCH "${target_arch}")
set(RINGOS_TARGET_TRIPLE "${target_triple}")
set(CMAKE_SYSTEM_NAME RingOS)
set(CMAKE_SYSTEM_PROCESSOR ${system_processor})
get_filename_component(RINGOS_TOOLCHAIN_ROOT "\${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(RINGOS_SHARE_DIR "\${RINGOS_TOOLCHAIN_ROOT}/share/ringos" ABSOLUTE)
set(RINGOS_TOOLCHAIN_VERSION "${toolchain_version}")
set(RINGOS_CLANG_RESOURCE_DIR "\${RINGOS_TOOLCHAIN_ROOT}/lib/clang/${clang_resource_version}")
get_filename_component(RINGOS_CMAKE_MODULE_DIR "\${RINGOS_TOOLCHAIN_ROOT}/cmake/modules" ABSOLUTE)
list(PREPEND CMAKE_MODULE_PATH "\${RINGOS_CMAKE_MODULE_DIR}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER "\${RINGOS_TOOLCHAIN_ROOT}/bin/${clang_name}")
set(CMAKE_CXX_COMPILER "\${RINGOS_TOOLCHAIN_ROOT}/bin/${clangxx_name}")
set(CMAKE_ASM_COMPILER "\${RINGOS_TOOLCHAIN_ROOT}/bin/${clang_name}")
set(CMAKE_C_COMPILER_TARGET "\${RINGOS_TARGET_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "\${RINGOS_TARGET_TRIPLE}")
set(CMAKE_ASM_COMPILER_TARGET "\${RINGOS_TARGET_TRIPLE}")
set(CMAKE_AR "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_ar_name}")
set(CMAKE_RANLIB "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_ranlib_name}")
set(CMAKE_C_COMPILER_AR "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_ar_name}")
set(CMAKE_C_COMPILER_RANLIB "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_ranlib_name}")
set(CMAKE_CXX_COMPILER_AR "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_ar_name}")
set(CMAKE_CXX_COMPILER_RANLIB "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_ranlib_name}")
set(CMAKE_OBJCOPY "\${RINGOS_TOOLCHAIN_ROOT}/bin/${llvm_objcopy_name}")
set(CMAKE_C_STANDARD_LIBRARIES_INIT "")
set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "")
set(CMAKE_ASM_STANDARD_LIBRARIES_INIT "")
set(CMAKE_C_FLAGS_INIT "--target=\"\${RINGOS_TARGET_TRIPLE}\" -resource-dir \"\${RINGOS_CLANG_RESOURCE_DIR}\"")
set(CMAKE_CXX_FLAGS_INIT "--target=\"\${RINGOS_TARGET_TRIPLE}\" -resource-dir \"\${RINGOS_CLANG_RESOURCE_DIR}\"")
set(CMAKE_ASM_FLAGS_INIT "--target=\"\${RINGOS_TARGET_TRIPLE}\" -resource-dir \"\${RINGOS_CLANG_RESOURCE_DIR}\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--target=\"\${RINGOS_TARGET_TRIPLE}\" -resource-dir \"\${RINGOS_CLANG_RESOURCE_DIR}\" -fuse-ld=lld")
EOF
}

write_generic_toolchain_file()
{
  local output_file="$1"

  cat > "${output_file}" <<'EOF'
if(NOT DEFINED RINGOS_TARGET_ARCH AND DEFINED CACHE{RINGOS_TARGET_ARCH})
  set(RINGOS_TARGET_ARCH "$CACHE{RINGOS_TARGET_ARCH}")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES RINGOS_TARGET_ARCH)
if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before loading ringos-toolchain.cmake.")
endif()
if(RINGOS_TARGET_ARCH STREQUAL "x64")
  include("${CMAKE_CURRENT_LIST_DIR}/ringos-x64-toolchain.cmake")
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  include("${CMAKE_CURRENT_LIST_DIR}/ringos-arm64-toolchain.cmake")
else()
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: ${RINGOS_TARGET_ARCH}")
endif()
EOF
}

package_installed_toolchain()
{
  local active_llvm_root="$1"
  local install_root="$2"
  local clang_path="${active_llvm_root}/bin/clang"
  local clangxx_path="${active_llvm_root}/bin/clang++"
  local lld_link_path="${active_llvm_root}/bin/lld-link"
  local ld_lld_path="${active_llvm_root}/bin/ld.lld"
  local llvm_ar_path="${active_llvm_root}/bin/llvm-ar"
  local llvm_ranlib_path="${active_llvm_root}/bin/llvm-ranlib"
  local llvm_objcopy_path="${active_llvm_root}/bin/llvm-objcopy"
  local llvm_lib_path="${active_llvm_root}/bin/llvm-lib"
  local clang_resource_dir=""
  local clang_resource_version=""
  local bundle_bin_dir="${install_root}/bin"
  local bundle_resource_dir=""
  local bundle_share_dir="${install_root}/share/ringos"
  local bundle_cmake_dir="${install_root}/cmake"
  local bundle_platform_dir="${bundle_cmake_dir}/modules/Platform"
  local clang_name=""
  local clangxx_name=""
  local lld_link_name=""
  local ld_lld_name=""
  local llvm_ar_name=""
  local llvm_ranlib_name=""
  local llvm_objcopy_name=""
  local llvm_lib_name=""

  for required_tool in \
    "${clang_path}" \
    "${clangxx_path}" \
    "${lld_link_path}" \
    "${ld_lld_path}" \
    "${llvm_ar_path}" \
    "${llvm_ranlib_path}" \
    "${llvm_objcopy_path}"; do
    if [[ ! -f "${required_tool}" ]]; then
      echo "Required tool is missing from ${active_llvm_root}: ${required_tool}" >&2
      exit 1
    fi
  done

  clang_resource_dir="$(resolve_clang_resource_dir "${clang_path}")"
  clang_resource_version="$(basename "${clang_resource_dir}")"
  bundle_resource_dir="${install_root}/lib/clang/${clang_resource_version}"

  clang_name="$(basename "${clang_path}")"
  clangxx_name="$(basename "${clangxx_path}")"
  lld_link_name="$(basename "${lld_link_path}")"
  ld_lld_name="$(basename "${ld_lld_path}")"
  llvm_ar_name="$(basename "${llvm_ar_path}")"
  llvm_ranlib_name="$(basename "${llvm_ranlib_path}")"
  llvm_objcopy_name="$(basename "${llvm_objcopy_path}")"

  if [[ -f "${llvm_lib_path}" ]]; then
    llvm_lib_name="$(basename "${llvm_lib_path}")"
  fi

  rm -rf "${install_root}"
  mkdir -p "${bundle_bin_dir}" "${bundle_share_dir}" "${bundle_platform_dir}" "${install_root}/lib/clang"

  cp -f "${clang_path}" "${bundle_bin_dir}/${clang_name}"
  cp -f "${clangxx_path}" "${bundle_bin_dir}/${clangxx_name}"
  cp -f "${lld_link_path}" "${bundle_bin_dir}/${lld_link_name}"
  cp -f "${ld_lld_path}" "${bundle_bin_dir}/${ld_lld_name}"
  cp -f "${llvm_ar_path}" "${bundle_bin_dir}/${llvm_ar_name}"
  cp -f "${llvm_ranlib_path}" "${bundle_bin_dir}/${llvm_ranlib_name}"
  cp -f "${llvm_objcopy_path}" "${bundle_bin_dir}/${llvm_objcopy_name}"

  if [[ -n "${llvm_lib_name}" ]]; then
    cp -f "${llvm_lib_path}" "${bundle_bin_dir}/${llvm_lib_name}"
  fi

  cp -R "${clang_resource_dir}" "${bundle_resource_dir}"
  cp -f "${repo_root}/tools/toolchain/modules/Platform/RingOS.cmake" "${bundle_platform_dir}/RingOS.cmake"

  write_arch_toolchain_file \
    x64 \
    x86_64-unknown-ringos-msvc \
    x86_64 \
    "${bundle_cmake_dir}/ringos-x64-toolchain.cmake" \
    "${clang_name}" \
    "${clangxx_name}" \
    "${llvm_ar_name}" \
    "${llvm_ranlib_name}" \
    "${llvm_objcopy_name}" \
    "${clang_resource_version}"

  write_arch_toolchain_file \
    arm64 \
    aarch64-unknown-ringos-msvc \
    aarch64 \
    "${bundle_cmake_dir}/ringos-arm64-toolchain.cmake" \
    "${clang_name}" \
    "${clangxx_name}" \
    "${llvm_ar_name}" \
    "${llvm_ranlib_name}" \
    "${llvm_objcopy_name}" \
    "${clang_resource_version}"

  write_generic_toolchain_file "${bundle_cmake_dir}/ringos-toolchain.cmake"

  cat > "${bundle_share_dir}/toolchain-version.txt" <<EOF
${toolchain_version}
EOF

  cat > "${bundle_share_dir}/toolchain-manifest.json" <<EOF
{
  "toolchain_version": "${toolchain_version}",
  "llvm_ref": "${llvm_ref}",
  "clang_resource_version": "${clang_resource_version}",
  "clang": "bin/${clang_name}",
  "clangxx": "bin/${clangxx_name}",
  "ld_lld": "bin/${ld_lld_name}",
  "lld_link": "bin/${lld_link_name}",
  "llvm_ar": "bin/${llvm_ar_name}",
  "llvm_ranlib": "bin/${llvm_ranlib_name}",
  "llvm_objcopy": "bin/${llvm_objcopy_name}"
}
EOF
}

output_archive=""
output_dir=""
release_repo="${GITHUB_REPOSITORY:-}"
github_token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
toolchain_version="${RINGOS_TOOLCHAIN_VERSION:-}"
normalized_patch_dir=""
extract_root=""
skip_bootstrap="${RINGOS_TOOLCHAIN_SKIP_BOOTSTRAP:-0}"
publish_release=0
release_tag=""
release_asset_name=""
release_version=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)
      release_repo="$2"
      shift 2
      ;;
    --output)
      output_archive="$2"
      shift 2
      ;;
    --output-dir)
      output_dir="$2"
      shift 2
      ;;
    --output-archive)
      output_archive="$2"
      shift 2
      ;;
    --version)
      toolchain_version="$2"
      shift 2
      ;;
    --publish)
      publish_release=1
      shift
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

if [[ -n "${output_dir}" && -n "${output_archive}" ]]; then
  echo "Specify either --output-dir or --output-archive, not both." >&2
  exit 1
fi

if [[ -z "${toolchain_version}" ]]; then
  if [[ -n "${release_repo}" ]]; then
    resolve_release_metadata "${release_repo}"
    toolchain_version="${release_version}"
  else
    toolchain_version="dev-local"
  fi
fi

if [[ -z "${release_tag}" ]]; then
  release_tag="ringos-toolchain-${toolchain_version}"
fi

if [[ -z "${release_asset_name}" ]]; then
  release_asset_name="${release_tag}.tar.xz"
fi

log_step "Preparing shared toolchain build for version ${toolchain_version}"

install_root="${repo_root}/build/toolchain"
toolchain_build_root="${repo_root}/build/toolchain-build"
build_llvm_root="${toolchain_build_root}/bootstrap-llvm"
bootstrap_toolchain_root="${build_llvm_root}/install"
staging_root="${toolchain_build_root}/package"
package_root="${staging_root}/ringos-toolchain"

mkdir -p "${repo_root}/build" "${toolchain_build_root}" "${build_llvm_root}"
rm -rf "${install_root}" "${staging_root}"
mkdir -p "${install_root}" "${staging_root}"

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

  log_step "Using ${bootstrap_compile_jobs} bootstrap compile job(s) and ${bootstrap_link_jobs} bootstrap link job(s)"
  build_bootstrap_llvm
fi

validate_packaged_toolchain()
{
  local bundle_root="$1"
  local -a expected_tools=(clang clang++ ld.lld lld-link llvm-ar llvm-ranlib llvm-objcopy)
  local -a expected_cmake_files=(ringos-toolchain.cmake ringos-x64-toolchain.cmake ringos-arm64-toolchain.cmake)
  local -a resource_dirs=()
  local tool_name=""
  local cmake_file=""

  for tool_name in "${expected_tools[@]}"; do
    if [[ ! -e "${bundle_root}/bin/${tool_name}" ]]; then
      echo "Packaged toolchain bundle is missing bin/${tool_name}." >&2
      exit 1
    fi
  done

  for cmake_file in "${expected_cmake_files[@]}"; do
    if [[ ! -e "${bundle_root}/cmake/${cmake_file}" ]]; then
      echo "Packaged toolchain bundle is missing cmake/${cmake_file}." >&2
      exit 1
    fi
  done

  if [[ ! -e "${bundle_root}/cmake/modules/Platform/RingOS.cmake" ]]; then
    echo "Packaged toolchain bundle is missing cmake/modules/Platform/RingOS.cmake." >&2
    exit 1
  fi

  if [[ ! -f "${bundle_root}/share/ringos/toolchain-version.txt" ]]; then
    echo "Packaged toolchain bundle is missing share/ringos/toolchain-version.txt." >&2
    exit 1
  fi

  if [[ ! -f "${bundle_root}/share/ringos/toolchain-manifest.json" ]]; then
    echo "Packaged toolchain bundle is missing share/ringos/toolchain-manifest.json." >&2
    exit 1
  fi

  shopt -s nullglob
  resource_dirs=("${bundle_root}/lib/clang"/*)
  shopt -u nullglob

  if [[ "${#resource_dirs[@]}" -eq 0 ]]; then
    echo "Packaged toolchain bundle is missing lib/clang/<version>." >&2
    exit 1
  fi

  if [[ ! -f "${resource_dirs[0]}/include/stddef.h" ]]; then
    echo "Packaged toolchain bundle is missing clang resource headers under ${resource_dirs[0]}." >&2
    exit 1
  fi
}

run_with_heartbeat "Packaging installed compiler bundle under ${install_root}" \
  package_installed_toolchain "${llvm_install_dir}" "${install_root}"

archive_stem="ringos-toolchain-${toolchain_version}"

if [[ -z "${output_archive}" ]]; then
  if [[ -z "${output_dir}" ]]; then
    output_dir="${repo_root}/build"
  fi

  output_archive="${output_dir}/${release_asset_name}"
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

log_step "Staging packaged compiler bundle under ${package_root}"
mkdir -p "${package_root}"
cp -a "${install_root}/." "${package_root}/"
run_with_heartbeat "Validating packaged compiler bundle" validate_packaged_toolchain "${package_root}"

rm -f "${output_archive_path}"
log_step "Writing versioned toolchain archive to ${output_archive_path}"
(
  cd "${staging_root}"
  cmake -E tar cJf "${output_archive_path}" ringos-toolchain
)

echo "Built shared toolchain archive: ${output_archive_path}"
echo "Toolchain version: ${toolchain_version}"
echo "clang in archive: ringos-toolchain/bin/clang"
echo "clang++ in archive: ringos-toolchain/bin/clang++"
echo "clang resource dir in archive: ringos-toolchain/lib/clang"
echo "generic toolchain file in archive: ringos-toolchain/cmake/ringos-toolchain.cmake"

if [[ "${publish_release}" == "1" ]]; then
  if [[ -z "${release_repo}" ]]; then
    echo "Set --repo or GITHUB_REPOSITORY before publishing a toolchain release." >&2
    exit 1
  fi

  need_tool gh

  if [[ -z "${github_token}" ]]; then
    echo "Set GH_TOKEN or GITHUB_TOKEN before publishing a toolchain release." >&2
    exit 1
  fi

  if gh release view "${release_tag}" --repo "${release_repo}" >/dev/null 2>&1; then
    log_step "Uploading toolchain archive to existing GitHub Release ${release_tag}"
    gh release upload "${release_tag}" "${output_archive_path}" --clobber --repo "${release_repo}"
  else
    log_step "Creating GitHub Release ${release_tag} and publishing the toolchain archive"
    gh release create "${release_tag}" \
      "${output_archive_path}" \
      --repo "${release_repo}" \
      --title "${release_tag}" \
      --notes "Shared ringos toolchain bundle version ${toolchain_version}."
  fi
fi

echo "release_repo=${release_repo}"
echo "release_version=${toolchain_version}"
echo "release_tag=${release_tag}"
echo "release_asset_name=${release_asset_name}"
echo "output_archive=${output_archive_path}"
