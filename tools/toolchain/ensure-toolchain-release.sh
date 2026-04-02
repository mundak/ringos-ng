#!/usr/bin/env bash
# Resolve the expected shared toolchain release for this checkout, download it when available,
# and optionally build or publish it in the dedicated toolchain workflow when the release does not exist yet.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

release_repo="${GITHUB_REPOSITORY:-}"
asset_name=""
publish_if_missing=0
allow_build=0
github_token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

default_install_root()
{
  if [[ -n "${LOCALAPPDATA:-}" ]]; then
    printf '%s\n' "${LOCALAPPDATA}/ringos/toolchain"
  elif [[ -n "${HOME:-}" ]]; then
    printf '%s\n' "${HOME}/.cache/ringos/toolchain"
  else
    printf '%s\n' "${repo_root}/build/installed-toolchain"
  fi
}

install_root="$(default_install_root)"

usage()
{
  cat <<EOF
Usage: tools/toolchain/ensure-toolchain-release.sh [options]

Options:
  --install-root <path>     Extract or build the shared toolchain bundle here.
  --repo <owner/name>       GitHub repository that owns the release assets.
  --allow-build             Build the toolchain archive locally when the release is missing.
  --publish-if-missing      Build and publish the release when it is missing. Requires gh and GH_TOKEN.
  --help                    Show this help text.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-root)
      install_root="$2"
      shift 2
      ;;
    --repo)
      release_repo="$2"
      shift 2
      ;;
    --allow-build)
      allow_build=1
      shift
      ;;
    --publish-if-missing)
      publish_if_missing=1
      shift
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

if [[ -z "${release_repo}" ]]; then
  echo "Set --repo or GITHUB_REPOSITORY before using toolchain releases." >&2
  exit 1
fi

need_tool()
{
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool not found: $1" >&2
    exit 1
  fi
}

need_tool cmake
need_tool curl
need_tool sha256sum
need_tool sed
need_tool mktemp

bash "${repo_root}/tools/llvm/ensure-libcxx-source.sh"

work_root="$(mktemp -d)"
trap 'rm -rf "${work_root}"' EXIT

toolchain_x64_id=""
toolchain_arm64_id=""
bundle_id=""
release_tag=""

read_manifest_value()
{
  local manifest_file="$1"
  local key="$2"

  sed -n "s/.*\"${key}\": \"\([^\"]*\)\".*/\1/p" "${manifest_file}" | head -n 1
}

compute_bundle_id()
{
  local x64_toolchain_id="$1"
  local arm64_toolchain_id="$2"
  local bundle_hash=""

  bundle_hash="$(printf 'x64=%s\narm64=%s\n' "${x64_toolchain_id}" "${arm64_toolchain_id}" | sha256sum)"
  bundle_hash="${bundle_hash%% *}"

  printf '%s\n' "${bundle_hash:0:20}"
}

resolve_expected_release_tag()
{
  local x64_build_dir="${work_root}/resolve-x64"
  local arm64_build_dir="${work_root}/resolve-arm64"

  cmake -S "${repo_root}" \
    -B "${x64_build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/toolchains/x64.cmake" \
    -DRINGOS_ENABLE_TESTING=OFF \
    -DRINGOS_TARGET_ARCH=x64 \
    -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${work_root}/install-root" >/dev/null

  cmake -S "${repo_root}" \
    -B "${arm64_build_dir}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/toolchains/arm64.cmake" \
    -DRINGOS_ENABLE_TESTING=OFF \
    -DRINGOS_TARGET_ARCH=arm64 \
    -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${work_root}/install-root" >/dev/null

  toolchain_x64_id="$(read_manifest_value "${x64_build_dir}/generated/installed_toolchain/x64/toolchain-manifest-x64.json" toolchain_id)"
  toolchain_arm64_id="$(read_manifest_value "${arm64_build_dir}/generated/installed_toolchain/arm64/toolchain-manifest-arm64.json" toolchain_id)"

  if [[ -z "${toolchain_x64_id}" || -z "${toolchain_arm64_id}" ]]; then
    echo "Unable to resolve the expected installed toolchain manifest IDs." >&2
    exit 1
  fi

  bundle_id="$(compute_bundle_id "${toolchain_x64_id}" "${toolchain_arm64_id}")"
  release_tag="ringos-toolchain-${bundle_id}"
  asset_name="${release_tag}.zip"
}

bundle_matches_expected_release()
{
  local bundle_root="$1"
  local manifest_x64="${bundle_root}/share/ringos/toolchain-manifest-x64.json"
  local manifest_arm64="${bundle_root}/share/ringos/toolchain-manifest-arm64.json"
  local actual_x64_id=""
  local actual_arm64_id=""

  if [[ ! -f "${manifest_x64}" || ! -f "${manifest_arm64}" ]]; then
    return 1
  fi

  actual_x64_id="$(read_manifest_value "${manifest_x64}" toolchain_id)"
  actual_arm64_id="$(read_manifest_value "${manifest_arm64}" toolchain_id)"

  [[ "${actual_x64_id}" == "${toolchain_x64_id}" && "${actual_arm64_id}" == "${toolchain_arm64_id}" ]]
}

install_archive()
{
  local archive_path="$1"
  local checksum_path="$2"
  local extract_root="${work_root}/extract"
  local extracted_bundle="${extract_root}/ringos-toolchain"

  if [[ -n "${checksum_path}" ]]; then
    (
      cd "$(dirname "${archive_path}")"
      sha256sum -c "$(basename "${checksum_path}")"
    )
  fi

  rm -rf "${extract_root}"
  mkdir -p "${extract_root}"
  (
    cd "${extract_root}"
    cmake -E tar xf "${archive_path}"
  )

  if [[ ! -d "${extracted_bundle}" ]]; then
    echo "Downloaded toolchain archive did not contain ringos-toolchain/." >&2
    exit 1
  fi

  mkdir -p "$(dirname "${install_root}")"
  rm -rf "${install_root}"
  cmake -E copy_directory "${extracted_bundle}" "${install_root}"
}

build_archive_locally()
{
  local archive_path="$1"
  bash "${repo_root}/tools/toolchain/build-toolchain.sh" "${archive_path}"
}

publish_release()
{
  local archive_path="$1"
  local checksum_path="$2"

  need_tool gh

  if [[ -z "${GH_TOKEN:-}" ]]; then
    echo "GH_TOKEN must be set before publishing a toolchain release." >&2
    exit 1
  fi

  if gh release view "${release_tag}" --repo "${release_repo}" >/dev/null 2>&1; then
    gh release upload "${release_tag}" \
      "${archive_path}#${asset_name}" \
      "${checksum_path}#${asset_name}.sha256" \
      --clobber \
      --repo "${release_repo}"
  else
    gh release create "${release_tag}" \
      "${archive_path}#${asset_name}" \
      "${checksum_path}#${asset_name}.sha256" \
      --repo "${release_repo}" \
      --title "${release_tag}" \
      --notes "Shared ringos toolchain bundle ${bundle_id}."
  fi
}

download_release_archive()
{
  local archive_path="$1"
  local checksum_path="$2"
  local base_url="https://github.com/${release_repo}/releases/download/${release_tag}"
  local -a curl_args=(--fail --location --retry 3 --silent --show-error)

  if [[ -n "${github_token}" ]]; then
    curl_args+=(-H "Authorization: Bearer ${github_token}")
  fi

  if ! curl "${curl_args[@]}" \
    --output "${archive_path}" \
    "${base_url}/${asset_name}"; then
    return 1
  fi

  curl "${curl_args[@]}" \
    --output "${checksum_path}" \
    "${base_url}/${asset_name}.sha256"
}

resolve_expected_release_tag

if bundle_matches_expected_release "${install_root}"; then
  echo "Using cached shared toolchain bundle at ${install_root}"
  echo "Expected release tag: ${release_tag}"
  echo "Expected release asset: ${asset_name}"
  exit 0
fi

downloaded_archive="${work_root}/${asset_name}"
downloaded_checksum="${work_root}/${asset_name}.sha256"

if download_release_archive "${downloaded_archive}" "${downloaded_checksum}"; then
  install_archive "${downloaded_archive}" "${downloaded_checksum}"

  if bundle_matches_expected_release "${install_root}"; then
    echo "Downloaded shared toolchain release ${release_tag} into ${install_root}"
    exit 0
  fi

  echo "Downloaded release ${release_tag}, but the extracted bundle does not match the expected manifests." >&2
  exit 1
fi

if [[ ${allow_build} -eq 0 && ${publish_if_missing} -eq 0 ]]; then
  echo "Release ${release_tag} is missing and local build fallback is disabled." >&2
  exit 1
fi

local_archive="${work_root}/${asset_name}"
local_checksum="${work_root}/${asset_name}.sha256"

build_archive_locally "${local_archive}"
(
  cd "${work_root}"
  sha256sum "${asset_name}" > "${asset_name}.sha256"
)

install_archive "${local_archive}" "${local_checksum}"

if [[ ${publish_if_missing} -eq 1 ]]; then
  publish_release "${local_archive}" "${local_checksum}"
fi

echo "Built shared toolchain bundle locally for release ${release_tag}"
