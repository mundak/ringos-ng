#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

log_step()
{
  printf '[toolchain-release][%s] %s\n' "$(date -u +%H:%M:%S)" "$*"
}

usage()
{
  cat <<EOF
Usage: tools/toolchain/run-toolchain-release.sh [options]

Options:
  --repo <owner/name>         Repository used for release version resolution and publish.
  --version <version>         Override the toolchain version instead of resolving it.
  --output-dir <path>         Directory that should receive the output archive.
  --output-archive <path>     Explicit output archive path.
  --publish                   Publish the built archive to GitHub Releases.
  --help                      Show this help text.
EOF
}

release_repo="${GITHUB_REPOSITORY:-}"
toolchain_version="${RINGOS_TOOLCHAIN_VERSION:-}"
output_dir=""
output_archive=""
publish_release=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)
      release_repo="$2"
      shift 2
      ;;
    --version)
      toolchain_version="$2"
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
    --publish)
      publish_release=1
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

if [[ -n "${output_dir}" && -n "${output_archive}" ]]; then
  echo "Specify either --output-dir or --output-archive, not both." >&2
  exit 1
fi

release_tag=""
release_asset_name=""

if [[ -z "${toolchain_version}" ]]; then
  if [[ -n "${release_repo}" ]]; then
    while IFS= read -r line; do
      case "${line}" in
        release_version=*)
          toolchain_version="${line#release_version=}"
          ;;
        release_tag=*)
          release_tag="${line#release_tag=}"
          ;;
        release_asset_name=*)
          release_asset_name="${line#release_asset_name=}"
          ;;
      esac
    done < <(bash "${repo_root}/tools/toolchain/resolve-toolchain-release-version.sh" --repo "${release_repo}")
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

if [[ -z "${output_archive}" ]]; then
  if [[ -z "${output_dir}" ]]; then
    output_dir="${repo_root}/build"
  fi

  output_archive="${output_dir}/${release_asset_name}"
fi

mkdir -p "$(dirname "${output_archive}")"

log_step "Resolved release version ${toolchain_version}"
log_step "Release tag ${release_tag}"
log_step "Toolchain archive target ${output_archive}"

build_args=(
  --version "${toolchain_version}"
  --output "${output_archive}")

log_step "Invoking tools/toolchain/build-toolchain.sh"
bash "${repo_root}/tools/toolchain/build-toolchain.sh" "${build_args[@]}"

if [[ "${publish_release}" == "1" ]]; then
  if [[ -z "${release_repo}" ]]; then
    echo "Set --repo or GITHUB_REPOSITORY before publishing a toolchain release." >&2
    exit 1
  fi

  if ! command -v gh >/dev/null 2>&1; then
    echo "Required tool not found: gh" >&2
    exit 1
  fi

  if [[ -z "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]]; then
    echo "Set GH_TOKEN or GITHUB_TOKEN before publishing a toolchain release." >&2
    exit 1
  fi

  if gh release view "${release_tag}" --repo "${release_repo}" >/dev/null 2>&1; then
    log_step "Uploading toolchain archive to existing GitHub Release ${release_tag}"
    gh release upload "${release_tag}" "${output_archive}" --clobber --repo "${release_repo}"
  else
    log_step "Creating GitHub Release ${release_tag} and publishing the toolchain archive"
    gh release create "${release_tag}" \
      "${output_archive}" \
      --repo "${release_repo}" \
      --title "${release_tag}" \
      --notes "Shared ringos toolchain bundle version ${toolchain_version}."
  fi
fi

log_step "Toolchain release flow completed"

echo "release_repo=${release_repo}"
echo "release_version=${toolchain_version}"
echo "release_tag=${release_tag}"
echo "release_asset_name=${release_asset_name}"
echo "output_archive=${output_archive}"
