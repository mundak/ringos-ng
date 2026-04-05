#!/usr/bin/env bash
# Extract a local shared toolchain archive from build/, or download the latest release there first.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

release_repo="${GITHUB_REPOSITORY:-}"
github_token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
archive_dir="${repo_root}/build"
install_root="${archive_dir}/toolchain"
install_root_explicit=0

usage()
{
  cat <<EOF
Usage: tools/toolchain/download-latest-toolchain.sh [options]

Options:
  --archive-dir <path>      Read or download toolchain archives here first.
  --install-root <path>     Extract the shared toolchain bundle here.
  --repo <owner/name>       GitHub repository that owns the release assets.
  --help                    Show this help text.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --archive-dir)
      archive_dir="$2"

      if [[ "${install_root_explicit}" != "1" ]]; then
        install_root="${archive_dir}/toolchain"
      fi

      shift 2
      ;;
    --install-root)
      install_root="$2"
      install_root_explicit=1
      shift 2
      ;;
    --repo)
      release_repo="$2"
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

need_tool()
{
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool not found: $1" >&2
    exit 1
  fi
}

need_tool cmake
need_tool python3
need_tool mktemp

work_root="$(mktemp -d)"
trap 'rm -rf "${work_root}"' EXIT

release_tag=""
release_version=""
asset_name=""
asset_api_url=""
asset_download_url=""

select_local_archive()
{
  local search_root="$1"

  python3 - "${search_root}" <<'PY'
import pathlib
import sys

archive_root = pathlib.Path(sys.argv[1])
archives = sorted(
    archive_root.glob("ringos-toolchain-*.tar.xz"),
    key=lambda entry: (entry.stat().st_mtime_ns, entry.name),
)

if archives:
    print(archives[-1])
PY
}

read_archive_version()
{
  local archive_path="$1"
  local archive_name=""

  archive_name="$(basename "${archive_path}")"

  if [[ "${archive_name}" =~ ^ringos-toolchain-(.+)\.tar\.xz$ ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
    return 0
  fi

  return 1
}

read_bundle_version()
{
  local bundle_root="$1"
  local version_file="${bundle_root}/share/ringos/toolchain-version.txt"

  if [[ ! -f "${version_file}" ]]; then
    return 1
  fi

  python3 - "${version_file}" <<'PY'
import pathlib
import sys

print(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip())
PY
}

cached_bundle_matches_release()
{
  local bundle_root="$1"
  local expected_version="$2"
  local actual_version=""

  if [[ ! -f "${bundle_root}/cmake/ringos-toolchain.cmake" ]]; then
    return 1
  fi

  actual_version="$(read_bundle_version "${bundle_root}")" || return 1
  [[ "${actual_version}" == "${expected_version}" ]]
}

load_latest_release_metadata()
{
  if [[ -z "${release_repo}" ]]; then
    echo "Set --repo or GITHUB_REPOSITORY before downloading toolchain releases." >&2
    exit 1
  fi

  need_tool curl

  local release_info_file="${work_root}/latest-release.json"
  local -a curl_args=(--fail --location --retry 3 --silent --show-error -H "Accept: application/vnd.github+json")

  if [[ -n "${github_token}" ]]; then
    curl_args+=(-H "Authorization: Bearer ${github_token}")
  fi

  curl "${curl_args[@]}" \
    --output "${release_info_file}" \
    "https://api.github.com/repos/${release_repo}/releases/latest"

  mapfile -t release_metadata < <(python3 - "${release_info_file}" <<'PY'
import json
import sys

release_path = sys.argv[1]

with open(release_path, "r", encoding="utf-8") as handle:
    release = json.load(handle)

tag_name = release.get("tag_name", "")
if not tag_name:
    sys.exit("Latest release metadata is missing tag_name")

toolchain_version = tag_name.removeprefix("ringos-toolchain-")
asset_name = f"{tag_name}.tar.xz"
asset = next((entry for entry in release.get("assets", []) if entry.get("name") == asset_name), None)

if asset is None:
  sys.exit(f"Latest release {tag_name} does not contain {asset_name}")

print(f"release_tag={tag_name}")
print(f"release_version={toolchain_version}")
print(f"asset_name={asset_name}")
print(f"asset_api_url={asset.get('url', '')}")
print(f"asset_download_url={asset.get('browser_download_url', '')}")
PY
  )

  for release_entry in "${release_metadata[@]}"; do
    case "${release_entry}" in
      release_tag=*)
        release_tag="${release_entry#release_tag=}"
        ;;
      release_version=*)
        release_version="${release_entry#release_version=}"
        ;;
      asset_name=*)
        asset_name="${release_entry#asset_name=}"
        ;;
      asset_api_url=*)
        asset_api_url="${release_entry#asset_api_url=}"
        ;;
      asset_download_url=*)
        asset_download_url="${release_entry#asset_download_url=}"
        ;;
    esac
  done

  if [[ -z "${release_tag}" || -z "${release_version}" || -z "${asset_name}" ]]; then
    echo "Unable to resolve the latest installed toolchain release metadata." >&2
    exit 1
  fi
}

install_archive()
{
  local archive_path="$1"
  local extract_root="${work_root}/extract"
  local extracted_bundle="${extract_root}/ringos-toolchain"

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

download_release_archive()
{
  local archive_path="$1"
  local -a curl_args=(--fail --location --retry 3 --silent --show-error)

  if [[ -n "${github_token}" && -n "${asset_api_url}" ]]; then
    curl_args+=(
      -H "Accept: application/octet-stream"
      -H "Authorization: Bearer ${github_token}")

    curl "${curl_args[@]}" \
      --output "${archive_path}" \
      "${asset_api_url}"
    return 0
  fi

  if [[ -n "${github_token}" ]]; then
    curl_args+=(-H "Authorization: Bearer ${github_token}")
  fi

  curl "${curl_args[@]}" \
    --output "${archive_path}" \
    "${asset_download_url}"
}

mkdir -p "${archive_dir}"

local_archive="$(select_local_archive "${archive_dir}")"

if [[ -n "${local_archive}" ]]; then
  archive_version="$(read_archive_version "${local_archive}")" || archive_version=""

  if [[ -n "${archive_version}" ]] && cached_bundle_matches_release "${install_root}" "${archive_version}"; then
    echo "Using extracted shared toolchain bundle at ${install_root}"
    echo "Toolchain archive: ${local_archive}"
    echo "Toolchain version: ${archive_version}"
    exit 0
  fi

  install_archive "${local_archive}"

  if [[ -n "${archive_version}" ]] && cached_bundle_matches_release "${install_root}" "${archive_version}"; then
    echo "Extracted shared toolchain archive ${local_archive} into ${install_root}"
    exit 0
  fi

  echo "Extracted archive ${local_archive}, but the extracted bundle version does not match ${archive_version}." >&2
  exit 1
fi

load_latest_release_metadata

downloaded_archive="${archive_dir}/${asset_name}"
download_release_archive "${downloaded_archive}"
install_archive "${downloaded_archive}"

if cached_bundle_matches_release "${install_root}" "${release_version}"; then
  echo "Downloaded shared toolchain release ${release_tag} into ${downloaded_archive}"
  echo "Extracted shared toolchain bundle into ${install_root}"
  exit 0
fi

echo "Downloaded release ${release_tag}, but the extracted bundle version does not match ${release_version}." >&2
exit 1
