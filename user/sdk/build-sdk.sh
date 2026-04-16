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
  local archive_search_root="$2"
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

  sequence_number="$(python3 - "${releases_json}" "${release_date}" "${archive_search_root}" <<'PY'
import json
import pathlib
import re
import sys

releases_path = sys.argv[1]
release_date = sys.argv[2]
archive_search_root = pathlib.Path(sys.argv[3])
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

if archive_search_root.exists():
  for archive in archive_search_root.glob(f"ringos-sdk-{release_date}.*.tar.xz"):
    match = pattern.match(archive.stem.removesuffix(".tar"))
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

read_toolchain_triple()
{
  sed -n 's/^set(RINGOS_TARGET_TRIPLE "\([^"]*\)").*/\1/p' \
    "${toolchain_root}/cmake/ringos-${1}-toolchain.cmake"
}

write_sdk_package_config()
{
  local package_dir="${install_root}/share/cmake/ringos_sdk"
  local package_file="${package_dir}/ringos_sdk-config.cmake"

  mkdir -p "${package_dir}"

  {
    cat <<EOF
get_filename_component(_ringos_sdk_root "\${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT DEFINED RINGOS_TARGET_TRIPLE)
  message(FATAL_ERROR "Configure your project with the RingOS toolchain file before loading ringos_sdk.")
endif()
set(RINGOS_SDK_ROOT "\${_ringos_sdk_root}")
set(_ringos_target_triple "\${RINGOS_TARGET_TRIPLE}")
get_filename_component(_ringos_sysroot_dir "\${_ringos_sdk_root}/sysroots/\${_ringos_target_triple}" ABSOLUTE)
get_filename_component(_ringos_include_dir "\${_ringos_sysroot_dir}/include" ABSOLUTE)
get_filename_component(_ringos_lib_dir "\${_ringos_sysroot_dir}/lib" ABSOLUTE)
if(NOT TARGET ringos-sdk)
  add_library(ringos-sdk STATIC IMPORTED GLOBAL)
  set_target_properties(ringos-sdk PROPERTIES IMPORTED_LOCATION "\${_ringos_lib_dir}/ringos_sdk.lib")
  target_include_directories(ringos-sdk INTERFACE "\${_ringos_include_dir}")
  set_target_properties(ringos-sdk PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C;CXX;ASM")
endif()
EOF
  } > "${package_file}"
}

build_sdk_for_arch()
{
  local target_arch="$1"
  local build_dir="$2"
  local target_triple="$3"

  log_step "Configuring ${target_arch} SDK"
  cmake -S "${repo_root}/user/sdk" \
    -B "${build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${toolchain_root}/cmake/ringos-toolchain.cmake" \
    -DRINGOS_TARGET_ARCH="${target_arch}" \
    -DRINGOS_SDK_STAGE_ROOT="${install_root}/sysroots/${target_triple}"

  log_step "Building ${target_arch} SDK"
  cmake --build "${build_dir}" --target ringos_sdk_stage --parallel "${build_jobs}"
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

version_search_dir="${repo_root}/build"

if [[ -n "${output_dir}" ]]; then
  version_search_dir="${output_dir}"
elif [[ -n "${output_archive}" ]]; then
  case "${output_archive}" in
    /*)
      version_search_dir="$(dirname "${output_archive}")"
      ;;
    *)
      version_search_dir="$(dirname "$PWD/${output_archive}")"
      ;;
  esac
fi

if [[ -z "${sdk_version}" ]]; then
  if [[ -n "${release_repo}" ]]; then
    resolve_release_metadata "${release_repo}" "${version_search_dir}"
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
need_tool mktemp
need_tool tar

log_step "Preparing shared SDK build for version ${sdk_version}"

work_root="$(mktemp -d)"
trap 'rm -rf "${work_root}"' EXIT

toolchain_root="${work_root}/toolchain"
install_root="${work_root}/sdk"
sdk_build_root="${work_root}/sdk-build"
staging_root="${sdk_build_root}/package"
package_root="${staging_root}/ringos-sdk"
x64_build_dir="${sdk_build_root}/x64"
arm64_build_dir="${sdk_build_root}/arm64"
build_jobs="${RINGOS_SDK_PAYLOAD_JOBS:-$(nproc 2>/dev/null || echo 1)}"

mkdir -p "${repo_root}/build" "${sdk_build_root}"
rm -rf "${install_root}" "${staging_root}" "${x64_build_dir}" "${arm64_build_dir}"
mkdir -p "${install_root}" "${staging_root}"

log_step "Preparing toolchain under ${toolchain_root} using archives from ${repo_root}/build"
bash "${repo_root}/tests/download-latest-toolchain.sh" \
  --repo "${release_repo}" \
  --archive-dir "${repo_root}/build" \
  --install-root "${toolchain_root}"

x64_triple="$(read_toolchain_triple x64)"
arm64_triple="$(read_toolchain_triple arm64)"

build_sdk_for_arch x64 "${x64_build_dir}" "${x64_triple}"
build_sdk_for_arch arm64 "${arm64_build_dir}" "${arm64_triple}"

write_sdk_package_config

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
