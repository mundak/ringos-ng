#!/usr/bin/env bash

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
  printf '[sdk][%s] %s\n' "$(date -u +%H:%M:%S)" "$*" >&2
}

resolve_job_count()
{
  local override="${1:-}"
  local cpu_count=""

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
    printf '%s\n' "${cpu_count}"
  else
    printf '1\n'
  fi
}

run_with_heartbeat()
{
  local description="$1"
  local heartbeat_pid=""
  local interval="${RINGOS_SDK_HEARTBEAT_SECONDS:-60}"
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

usage()
{
  cat <<EOF
Usage: user/sdk/build-sdk.sh [options] [output-archive]

Options:
  --repo <owner/name>        GitHub repository that owns the toolchain release history.
  --output <path>            Output archive path.
  --output-dir <path>        Directory that should receive the output archive.
  --output-archive <path>    Explicit output archive path.
  --version <version>        SDK version recorded in the bundle metadata.
  --publish                  Publish the built archive to GitHub Releases.
  --help                     Show this help text.
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
    echo "Set --repo or GITHUB_REPOSITORY before resolving an SDK release version." >&2
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
pattern = re.compile(rf"^ringos-sdk-{re.escape(release_date)}\.(\d+)$")
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
  release_tag="ringos-sdk-${release_version}"
  release_asset_name="${release_tag}.tar.xz"
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

resolve_sdk_target_triple()
{
  case "$1" in
    x64)
      printf '%s\n' "x86_64-unknown-ringos-msvc"
      ;;
    arm64)
      printf '%s\n' "aarch64-unknown-ringos-msvc"
      ;;
    *)
      echo "Unsupported RingOS SDK target architecture: $1" >&2
      exit 1
      ;;
  esac
}

populate_sdk_link_flags()
{
  local target_arch="$1"
  local out_var_name="$2"
  local -n out_ref="${out_var_name}"

  out_ref=(
    -fuse-ld=lld
    -nostdlib
    -Wl,/entry:user_start
    -Wl,/subsystem:console
    -Wl,/nodefaultlib
    -Wl,/filealign:4096)

  case "${target_arch}" in
    x64)
      out_ref+=(
        -Wl,/fixed
        -Wl,/dynamicbase:no
        -Wl,/base:0x400000)
      ;;
    arm64)
      out_ref+=(
        -Wl,/fixed:no
        -Wl,/base:0x400000)
      ;;
    *)
      echo "Unsupported RingOS SDK target architecture: ${target_arch}" >&2
      exit 1
      ;;
  esac
}

resolve_clang_resource_version()
{
  local clang_path="$1"
  local resource_dir=""

  resource_dir="$(${clang_path} --print-resource-dir)"

  if [[ -z "${resource_dir}" || ! -d "${resource_dir}" ]]; then
    echo "Unable to resolve clang resource directory from ${clang_path}." >&2
    exit 1
  fi

  basename "${resource_dir}"
}

prepare_libcxx_include_dir()
{
  local llvm_repo_url="${RINGOS_LLVM_REPO_URL:-https://github.com/llvm/llvm-project.git}"
  local llvm_ref="${RINGOS_LLVM_REF:-3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff}"
  local llvm_repo_archive_base="${llvm_repo_url%.git}"
  local llvm_source_archive_url="${RINGOS_LLVM_SOURCE_ARCHIVE_URL:-${llvm_repo_archive_base}/archive/${llvm_ref}.tar.gz}"
  local llvm_download_dir="${RINGOS_LLVM_DOWNLOAD_DIR:-${repo_root}/build/toolchain-build/bootstrap-llvm/downloads}"
  local llvm_source_archive="${RINGOS_LLVM_SOURCE_ARCHIVE:-${llvm_download_dir}/llvm-project-${llvm_ref}.tar.gz}"
  local source_dir_name=""
  local prepared_source_root=""
  local archive_tmp=""
  local extract_root=""
  local extracted_root_name=""
  local extracted_source_dir=""
  local first_archive_entry=""

  if [[ -n "${RINGOS_LIBCXX_INCLUDE_DIR:-}" && -f "${RINGOS_LIBCXX_INCLUDE_DIR}/__config" ]]; then
    printf '%s\n' "${RINGOS_LIBCXX_INCLUDE_DIR}"
    return
  fi

  source_dir_name="$(archive_stem_from_path "${llvm_source_archive}")"
  prepared_source_root="${repo_root}/build/toolchain-build/bootstrap-llvm/src/${source_dir_name}"

  if [[ -f "${prepared_source_root}/libcxx/include/__config" ]]; then
    printf '%s\n' "${prepared_source_root}/libcxx/include"
    return
  fi

  need_tool curl

  mkdir -p "${llvm_download_dir}" "${sdk_build_root}/llvm-source"

  if [[ ! -f "${llvm_source_archive}" ]]; then
    archive_tmp="${llvm_source_archive}.tmp"
    rm -f "${archive_tmp}"

    run_with_heartbeat "Downloading LLVM source archive ${llvm_ref}" \
      curl --fail --location --retry 3 --silent --show-error \
        --output "${archive_tmp}" \
        "${llvm_source_archive_url}"

    mv "${archive_tmp}" "${llvm_source_archive}"
  fi

  prepared_source_root="${sdk_build_root}/llvm-source/${source_dir_name}"

  if [[ ! -f "${prepared_source_root}/libcxx/include/__config" ]]; then
    extract_root="$(mktemp -d)"
    first_archive_entry="$(tar -tzf "${llvm_source_archive}" | awk 'NR == 1 { print; found = 1 } END { if (!found) exit 1 }')"
    extracted_root_name="${first_archive_entry%%/*}"

    if [[ -z "${extracted_root_name}" ]]; then
      echo "Unable to determine the top-level directory in ${llvm_source_archive}." >&2
      rm -rf "${extract_root}"
      exit 1
    fi

    rm -rf "${prepared_source_root}"
    tar -xzf "${llvm_source_archive}" -C "${extract_root}"
    extracted_source_dir="${extract_root}/${extracted_root_name}"

    if [[ ! -d "${extracted_source_dir}" ]]; then
      echo "Expected extracted LLVM source directory ${extracted_source_dir} is missing." >&2
      rm -rf "${extract_root}"
      exit 1
    fi

    mv "${extracted_source_dir}" "${prepared_source_root}"
    rm -rf "${extract_root}"
  fi

  if [[ ! -f "${prepared_source_root}/libcxx/include/__config" ]]; then
    echo "Unable to locate libc++ headers under ${prepared_source_root}." >&2
    exit 1
  fi

  printf '%s\n' "${prepared_source_root}/libcxx/include"
}

write_sdk_arch_metadata()
{
  local target_arch="$1"
  local target_triple="$2"
  local clang_resource_version="$3"
  local share_dir="${install_root}/share/ringos"
  local compile_cfg="${share_dir}/compile-${target_arch}.cfg"
  local link_cfg="${share_dir}/link-${target_arch}.cfg"
  local manifest_file="${share_dir}/sdk-manifest-${target_arch}.json"
  local runtime_manifest_file="${share_dir}/runtime-manifest-${target_arch}.txt"
  local link_flags=()
  local link_flag=""

  mkdir -p "${share_dir}"

  cat > "${compile_cfg}" <<EOF
--target=${target_triple}
-fno-stack-protector
-fno-builtin
-resource-dir
<CFGDIR>/../../../toolchain/lib/clang/${clang_resource_version}
-I
<CFGDIR>/../../sysroots/${target_triple}/include
EOF

  populate_sdk_link_flags "${target_arch}" link_flags

  {
    printf '%s\n' "--target=${target_triple}"
    printf '%s\n' "-resource-dir"
    printf '%s\n' "<CFGDIR>/../../../toolchain/lib/clang/${clang_resource_version}"

    for link_flag in "${link_flags[@]}"; do
      printf '%s\n' "${link_flag}"
    done

    printf '%s\n' "<CFGDIR>/../../sysroots/${target_triple}/lib/crt0.obj"
    printf '%s\n' "<CFGDIR>/../../sysroots/${target_triple}/lib/ringos_c.lib"
    printf '%s\n' "<CFGDIR>/../../sysroots/${target_triple}/lib/ringos_sdk.lib"
    printf '%s\n' "<CFGDIR>/../../sysroots/${target_triple}/lib/clang_rt.builtins.lib"
  } > "${link_cfg}"

  cat > "${runtime_manifest_file}" <<EOF
sdk_version=${sdk_version}
toolchain_version=${toolchain_version}
target_arch=${target_arch}
target_triple=${target_triple}
package_config=share/cmake/ringos_sdk/ringos_sdk-config.cmake
sysroot=sysroots/${target_triple}
crt0=sysroots/${target_triple}/lib/crt0.obj
sdk=sysroots/${target_triple}/lib/ringos_sdk.lib
libc=sysroots/${target_triple}/lib/ringos_c.lib
compiler_rt=sysroots/${target_triple}/lib/clang_rt.builtins.lib
EOF

  cat > "${manifest_file}" <<EOF
{
  "sdk_version": "${sdk_version}",
  "toolchain_version": "${toolchain_version}",
  "target_arch": "${target_arch}",
  "target_triple": "${target_triple}",
  "package_config": "share/cmake/ringos_sdk/ringos_sdk-config.cmake",
  "sysroot": "sysroots/${target_triple}"
}
EOF
}

write_sdk_package_config()
{
  local clang_resource_version="$1"
  local package_dir="${install_root}/share/cmake/ringos_sdk"
  local package_file="${package_dir}/ringos_sdk-config.cmake"
  local x64_link_flags=()
  local arm64_link_flags=()
  local link_flag=""

  mkdir -p "${package_dir}"

  populate_sdk_link_flags x64 x64_link_flags
  populate_sdk_link_flags arm64 arm64_link_flags

  {
    cat <<EOF
get_filename_component(_ringos_sdk_root "\${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT DEFINED RINGOS_TARGET_ARCH AND DEFINED CACHE{RINGOS_TARGET_ARCH})
  set(RINGOS_TARGET_ARCH "\$CACHE{RINGOS_TARGET_ARCH}")
endif()
if(NOT DEFINED RINGOS_TARGET_ARCH)
  message(FATAL_ERROR "Set RINGOS_TARGET_ARCH to 'x64' or 'arm64' before loading ringos_sdk.")
endif()
if(RINGOS_TARGET_ARCH STREQUAL "x64")
  set(_ringos_target_triple "x86_64-unknown-ringos-msvc")
  set(_ringos_link_options
EOF

    for link_flag in "${x64_link_flags[@]}"; do
      printf '    %s\n' "${link_flag}"
    done

    cat <<EOF
  )
elseif(RINGOS_TARGET_ARCH STREQUAL "arm64")
  set(_ringos_target_triple "aarch64-unknown-ringos-msvc")
  set(_ringos_link_options
EOF

    for link_flag in "${arm64_link_flags[@]}"; do
      printf '    %s\n' "${link_flag}"
    done

    cat <<EOF
  )
else()
  message(FATAL_ERROR "Unsupported RINGOS_TARGET_ARCH: \${RINGOS_TARGET_ARCH}")
endif()
set(RINGOS_SDK_ROOT "\${_ringos_sdk_root}")
get_filename_component(_ringos_sysroot_dir "\${_ringos_sdk_root}/sysroots/\${_ringos_target_triple}" ABSOLUTE)
get_filename_component(_ringos_include_dir "\${_ringos_sysroot_dir}/include" ABSOLUTE)
get_filename_component(_ringos_cxx_include_dir "\${_ringos_include_dir}/c++/v1" ABSOLUTE)
get_filename_component(_ringos_lib_dir "\${_ringos_sysroot_dir}/lib" ABSOLUTE)
foreach(_ringos_required_path
    "\${_ringos_lib_dir}/crt0.obj"
    "\${_ringos_lib_dir}/ringos_sdk.lib"
    "\${_ringos_lib_dir}/ringos_c.lib"
    "\${_ringos_lib_dir}/clang_rt.builtins.lib"
    "\${_ringos_include_dir}")
  if(NOT EXISTS "\${_ringos_required_path}")
    message(FATAL_ERROR "RingOS SDK package is incomplete: missing \${_ringos_required_path}.")
  endif()
endforeach()
if(NOT TARGET ringos-sdk)
  add_library(ringos-sdk STATIC IMPORTED GLOBAL)
  set_target_properties(ringos-sdk PROPERTIES IMPORTED_LOCATION "\${_ringos_lib_dir}/ringos_sdk.lib")
  target_include_directories(ringos-sdk INTERFACE "\${_ringos_include_dir}")
  target_compile_options(ringos-sdk INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
    "$<$<COMPILE_LANGUAGE:CXX>:-isystem>"
    "$<$<COMPILE_LANGUAGE:CXX>:\${_ringos_cxx_include_dir}>")
  target_link_options(ringos-sdk INTERFACE
    -fuse-ld=lld
    -nostdlib
    -Wl,/entry:user_start
    -Wl,/subsystem:console
    -Wl,/nodefaultlib
    -Wl,/filealign:4096
    \${_ringos_link_options})
  target_link_libraries(ringos-sdk INTERFACE
    "\${_ringos_lib_dir}/crt0.obj"
    "\${_ringos_lib_dir}/ringos_c.lib"
    "\${_ringos_lib_dir}/clang_rt.builtins.lib")
  set_target_properties(ringos-sdk PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C;CXX;ASM")
endif()
EOF
  } > "${package_file}"
}

stage_sdk_payload_for_arch()
{
  local target_arch="$1"
  local build_dir="$2"
  local libcxx_include_dir="$3"
  local target_triple=""
  local stage_root=""

  target_triple="$(resolve_sdk_target_triple "${target_arch}")"
  stage_root="${install_root}/sysroots/${target_triple}"

  run_with_heartbeat "Configuring ${target_arch} SDK runtime build in ${build_dir}" \
    cmake -S "${repo_root}/user/sdk" \
      -B "${build_dir}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="${toolchain_root}/cmake/ringos-toolchain.cmake" \
      -DRINGOS_TARGET_ARCH="${target_arch}" \
      -DRINGOS_SDK_STAGE_ROOT="${stage_root}" \
      -DRINGOS_LIBCXX_INCLUDE_DIR="${libcxx_include_dir}" \
      -DRINGOS_LIBCXX_OVERLAY_DIR="${repo_root}/user/libcxx"

  run_with_heartbeat "Building ${target_arch} SDK runtime artifacts" \
    cmake --build "${build_dir}" --target ringos_sdk_stage --parallel "${payload_build_jobs}"

  write_sdk_arch_metadata "${target_arch}" "${target_triple}" "${clang_resource_version}"
}

validate_packaged_sdk()
{
  local bundle_root="$1"
  local -a expected_target_triples=(x86_64-unknown-ringos-msvc aarch64-unknown-ringos-msvc)
  local -a expected_libcxx_headers=(__config __config_site __assertion_handler cstddef cstdint type_traits)
  local target_triple=""
  local libcxx_header=""

  if [[ ! -e "${bundle_root}/share/cmake/ringos_sdk/ringos_sdk-config.cmake" ]]; then
    echo "Packaged SDK bundle is missing share/cmake/ringos_sdk/ringos_sdk-config.cmake." >&2
    exit 1
  fi

  if [[ ! -f "${bundle_root}/share/ringos/sdk-version.txt" ]]; then
    echo "Packaged SDK bundle is missing share/ringos/sdk-version.txt." >&2
    exit 1
  fi

  for target_triple in "${expected_target_triples[@]}"; do
    for required_path in \
      "sysroots/${target_triple}/lib/crt0.obj" \
      "sysroots/${target_triple}/lib/ringos_sdk.lib" \
      "sysroots/${target_triple}/lib/ringos_c.lib" \
      "sysroots/${target_triple}/lib/clang_rt.builtins.lib"; do
      if [[ ! -e "${bundle_root}/${required_path}" ]]; then
        echo "Packaged SDK bundle is missing ${required_path}." >&2
        exit 1
      fi
    done

    for libcxx_header in "${expected_libcxx_headers[@]}"; do
      if [[ ! -e "${bundle_root}/sysroots/${target_triple}/include/c++/v1/${libcxx_header}" ]]; then
        echo "Packaged SDK bundle is missing libc++ header ${libcxx_header} for ${target_triple}." >&2
        exit 1
      fi
    done
  done
}

output_archive=""
output_dir=""
sdk_version="${RINGOS_SDK_VERSION:-}"
release_repo="${GITHUB_REPOSITORY:-}"
github_token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
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
      sdk_version="$2"
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

if [[ -z "${sdk_version}" ]]; then
  if [[ -n "${release_repo}" ]]; then
    resolve_release_metadata "${release_repo}"
    sdk_version="${release_version}"
  else
    sdk_version="dev-local"
  fi
fi

if [[ -z "${release_tag}" ]]; then
  release_tag="ringos-sdk-${sdk_version}"
fi

if [[ -z "${release_asset_name}" ]]; then
  release_asset_name="${release_tag}.tar.xz"
fi

need_tool cmake
need_tool tar

log_step "Preparing shared SDK build for version ${sdk_version}"

toolchain_root="${repo_root}/build/toolchain"
install_root="${repo_root}/build/sdk"
sdk_build_root="${repo_root}/build/sdk-build"
staging_root="${sdk_build_root}/package"
package_root="${staging_root}/ringos-sdk"
x64_build_dir="${sdk_build_root}/x64"
arm64_build_dir="${sdk_build_root}/arm64"
payload_build_jobs="$(resolve_job_count "${RINGOS_SDK_PAYLOAD_JOBS:-}")"

mkdir -p "${repo_root}/build" "${sdk_build_root}"
rm -rf "${install_root}" "${staging_root}" "${x64_build_dir}" "${arm64_build_dir}"
mkdir -p "${install_root}" "${staging_root}"

run_with_heartbeat "Resolving published toolchain bundle into ${toolchain_root}" \
  bash "${repo_root}/tests/download-latest-toolchain.sh" \
    --repo "${release_repo}" \
    --archive-dir "${repo_root}/build" \
    --install-root "${toolchain_root}"

toolchain_version_file="${toolchain_root}/share/ringos/toolchain-version.txt"
if [[ ! -f "${toolchain_version_file}" ]]; then
  echo "Resolved toolchain bundle is missing ${toolchain_version_file}." >&2
  exit 1
fi

toolchain_version="$(tr -d '\r\n' < "${toolchain_version_file}")"

clang_resource_version="$(resolve_clang_resource_version "${toolchain_root}/bin/clang")"
libcxx_include_dir="$(prepare_libcxx_include_dir)"

stage_sdk_payload_for_arch x64 "${x64_build_dir}" "${libcxx_include_dir}"
stage_sdk_payload_for_arch arm64 "${arm64_build_dir}" "${libcxx_include_dir}"

write_sdk_package_config "${clang_resource_version}"

sdk_version_file="${install_root}/share/ringos/sdk-version.txt"
mkdir -p "$(dirname "${sdk_version_file}")"
printf '%s\n' "${sdk_version}" > "${sdk_version_file}"

archive_stem="ringos-sdk-${sdk_version}"

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

log_step "Staging packaged SDK bundle under ${package_root}"
mkdir -p "${package_root}"
cp -a "${install_root}/." "${package_root}/"
run_with_heartbeat "Validating packaged SDK bundle" validate_packaged_sdk "${package_root}"

rm -f "${output_archive_path}"
log_step "Writing versioned SDK archive to ${output_archive_path}"
(
  cd "${staging_root}"
  cmake -E tar cJf "${output_archive_path}" ringos-sdk
)

echo "Built shared SDK archive: ${output_archive_path}"
echo "SDK version: ${sdk_version}"
echo "SDK package config in archive: ringos-sdk/share/cmake/ringos_sdk/ringos_sdk-config.cmake"

if [[ "${publish_release}" == "1" ]]; then
  if [[ -z "${release_repo}" ]]; then
    echo "Set --repo or GITHUB_REPOSITORY before publishing an SDK release." >&2
    exit 1
  fi

  need_tool gh

  if [[ -z "${github_token}" ]]; then
    echo "Set GH_TOKEN or GITHUB_TOKEN before publishing an SDK release." >&2
    exit 1
  fi

  if gh release view "${release_tag}" --repo "${release_repo}" >/dev/null 2>&1; then
    log_step "Uploading SDK archive to existing GitHub Release ${release_tag}"
    gh release upload "${release_tag}" "${output_archive_path}" --clobber --repo "${release_repo}"
  else
    log_step "Creating GitHub Release ${release_tag} and publishing the SDK archive"
    gh release create "${release_tag}" \
      "${output_archive_path}" \
      --repo "${release_repo}" \
      --title "${release_tag}" \
      --notes "Shared ringos SDK bundle version ${sdk_version}."
  fi
fi

echo "release_repo=${release_repo}"
echo "release_version=${sdk_version}"
echo "release_tag=${release_tag}"
echo "release_asset_name=${release_asset_name}"
echo "output_archive=${output_archive_path}"
