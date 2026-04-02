#!/usr/bin/env bash
# Build the shared ringos toolchain package for all targets and archive it as a distributable ZIP.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

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

if [[ $# -ge 1 ]]; then
  output_zip="$1"
else
  output_zip=""
fi

install_root="$(mktemp -d)"
staging_root="$(mktemp -d)"
package_root="${staging_root}/ringos-toolchain"
x64_build_dir="${staging_root}/build-x64"
arm64_build_dir="${staging_root}/build-arm64"

bash "${repo_root}/tools/llvm/ensure-libcxx-source.sh"

cleanup()
{
  rm -rf "${install_root}" "${staging_root}"
}

trap cleanup EXIT

cmake -S "${repo_root}" \
  -B "${x64_build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DRINGOS_ENABLE_TESTING=ON \
  -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/toolchains/x64.cmake" \
  -DRINGOS_TARGET_ARCH=x64 \
  -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${install_root}"
cmake --build "${x64_build_dir}" --target ringos_installed_toolchain

cmake -S "${repo_root}" \
  -B "${arm64_build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DRINGOS_ENABLE_TESTING=ON \
  -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/toolchains/arm64.cmake" \
  -DRINGOS_TARGET_ARCH=arm64 \
  -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${install_root}"
cmake --build "${arm64_build_dir}" --target ringos_installed_toolchain

x64_manifest="${install_root}/share/ringos/toolchain-manifest-x64.json"
arm64_manifest="${install_root}/share/ringos/toolchain-manifest-arm64.json"
x64_toolchain_id="$(read_manifest_value "${x64_manifest}" toolchain_id)"
arm64_toolchain_id="$(read_manifest_value "${arm64_manifest}" toolchain_id)"

if [[ -z "${x64_toolchain_id}" || -z "${arm64_toolchain_id}" ]]; then
  echo "Unable to resolve installed toolchain manifest IDs after packaging." >&2
  exit 1
fi

bundle_id="$(compute_bundle_id "${x64_toolchain_id}" "${arm64_toolchain_id}")"
bundle_manifest="${install_root}/share/ringos/toolchain-bundle-manifest.json"
bundle_id_file="${install_root}/share/ringos/toolchain-bundle-id.txt"

cat > "${bundle_manifest}" <<EOF
{
  "bundle_id": "${bundle_id}",
  "x64_toolchain_id": "${x64_toolchain_id}",
  "arm64_toolchain_id": "${arm64_toolchain_id}"
}
EOF

printf '%s\n' "${bundle_id}" > "${bundle_id_file}"

archive_stem="ringos-toolchain-${bundle_id}"

if [[ -z "${output_zip}" ]]; then
  output_zip="${repo_root}/build/${archive_stem}.zip"
fi

case "${output_zip}" in
  /*)
    output_zip_path="${output_zip}"
    ;;
  *)
    output_zip_path="$PWD/${output_zip}"
    ;;
esac

mkdir -p "$(dirname "${output_zip_path}")"

mkdir -p "${package_root}"
cp -R "${install_root}/." "${package_root}/"

rm -f "${output_zip_path}"
(
  cd "${staging_root}"
  cmake -E tar cf "${output_zip_path}" --format=zip ringos-toolchain
)

echo "Built shared toolchain archive: ${output_zip_path}"
echo "Bundle id: ${bundle_id}"
echo "x64 toolchain file in archive: ringos-toolchain/cmake/ringos-x64-toolchain.cmake"
echo "arm64 toolchain file in archive: ringos-toolchain/cmake/ringos-arm64-toolchain.cmake"
echo "generic toolchain file in archive: ringos-toolchain/cmake/ringos-toolchain.cmake"
