#!/usr/bin/env bash
# Resolve the latest published toolchain version, prefer the newest same-or-newer local archive in build/, or download it there first.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

release_repo="${GITHUB_REPOSITORY:-}"
github_token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
archive_dir="${repo_root}/build"
install_root="${archive_dir}/toolchain"
install_root_explicit=0

usage()
{
  cat <<EOF
Usage: tests/download-latest-toolchain.sh [options]

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
import re
import sys

archive_root = pathlib.Path(sys.argv[1])
pattern = re.compile(r"^ringos-toolchain-(.+)\.tar\.xz$")
selected_archive = None
selected_version_key = None

for archive in archive_root.glob("ringos-toolchain-*.tar.xz"):
  match = pattern.match(archive.name)
  if match is None:
    continue

  try:
    version_key = tuple(int(part) for part in match.group(1).split("."))
  except ValueError:
    continue

  if selected_archive is None or version_key > selected_version_key or (
    version_key == selected_version_key and archive.name > selected_archive.name
  ):
    selected_archive = archive
    selected_version_key = version_key

if selected_archive is not None:
  print(selected_archive)
PY
}

version_is_same_or_newer()
{
  local candidate_version="$1"
  local baseline_version="$2"

  python3 - "${candidate_version}" "${baseline_version}" <<'PY'
import sys

def parse_version(value: str) -> tuple[int, ...]:
  return tuple(int(part) for part in value.split("."))

candidate = parse_version(sys.argv[1])
baseline = parse_version(sys.argv[2])
sys.exit(0 if candidate >= baseline else 1)
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

load_latest_release_metadata()
{
  if [[ -z "${release_repo}" ]]; then
    echo "Set --repo or GITHUB_REPOSITORY before downloading toolchain releases." >&2
    exit 1
  fi

  need_tool curl

  local release_info_file="${work_root}/toolchain-releases.json"
  local -a curl_args=(--fail --location --retry 3 --silent --show-error -H "Accept: application/vnd.github+json")

  if [[ -n "${github_token}" ]]; then
    curl_args+=(-H "Authorization: Bearer ${github_token}")
  fi

  curl "${curl_args[@]}" \
    --output "${release_info_file}" \
    "https://api.github.com/repos/${release_repo}/releases?per_page=100"

  mapfile -t release_metadata < <(python3 - "${release_info_file}" <<'PY'
import json
import re
import sys

release_path = sys.argv[1]

with open(release_path, "r", encoding="utf-8") as handle:
    releases = json.load(handle)

pattern = re.compile(r"^ringos-toolchain-(.+)$")
selected_release = None
selected_version_key = None

for release in releases:
    tag_name = release.get("tag_name", "")
    match = pattern.match(tag_name)
    if match is None:
        continue

    version = match.group(1)
    try:
        version_key = tuple(int(part) for part in version.split("."))
    except ValueError:
        continue

    asset_name = f"{tag_name}.tar.xz"
    asset = next((entry for entry in release.get("assets", []) if entry.get("name") == asset_name), None)
    if asset is None:
        continue

    if selected_release is None or version_key > selected_version_key:
        selected_release = release
        selected_version_key = version_key

if selected_release is None:
    sys.exit("Unable to locate a published ringos-toolchain release asset")

tag_name = selected_release.get("tag_name", "")
toolchain_version = tag_name.removeprefix("ringos-toolchain-")
asset_name = f"{tag_name}.tar.xz"
asset = next((entry for entry in selected_release.get("assets", []) if entry.get("name") == asset_name), None)

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

  archive_path="$(cd "$(dirname "${archive_path}")" && pwd)/$(basename "${archive_path}")"

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

load_latest_release_metadata

local_archive="$(select_local_archive "${archive_dir}")"
local_archive_version=""

if [[ -n "${local_archive}" ]]; then
  local_archive_version="$(read_archive_version "${local_archive}")" || local_archive_version=""

  if [[ -n "${local_archive_version}" ]] && version_is_same_or_newer "${local_archive_version}" "${release_version}"; then
    install_archive "${local_archive}"

    echo "Extracted shared toolchain archive ${local_archive} into ${install_root}"
    echo "Toolchain version: ${local_archive_version}"
    exit 0
  fi
fi

downloaded_archive="${archive_dir}/${asset_name}"
echo "Downloading published toolchain archive ${asset_name} into ${archive_dir}"
download_release_archive "${downloaded_archive}"
install_archive "${downloaded_archive}"

echo "Downloaded shared toolchain release ${release_tag} into ${downloaded_archive}"
echo "Extracted shared toolchain bundle into ${install_root}"
