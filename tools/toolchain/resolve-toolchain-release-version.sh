#!/usr/bin/env bash

set -euo pipefail

release_repo="${GITHUB_REPOSITORY:-}"
github_token="${GH_TOKEN:-${GITHUB_TOKEN:-}}"

usage()
{
  cat <<EOF
Usage: tools/toolchain/resolve-toolchain-release-version.sh [options]

Options:
  --repo <owner/name>      GitHub repository that owns the release history.
  --help                   Show this help text.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
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

if [[ -z "${release_repo}" ]]; then
  echo "Set --repo or GITHUB_REPOSITORY before resolving a toolchain release version." >&2
  exit 1
fi

need_tool()
{
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool not found: $1" >&2
    exit 1
  fi
}

need_tool curl
need_tool python3
need_tool date

release_date="$(date -u +%Y.%m.%d)"
releases_json="$(mktemp)"
trap 'rm -f "${releases_json}"' EXIT

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

release_version="${release_date}.${sequence_number}"
release_tag="ringos-toolchain-${release_version}"
release_asset_name="${release_tag}.tar.xz"

echo "release_version=${release_version}"
echo "release_tag=${release_tag}"
echo "release_asset_name=${release_asset_name}"
