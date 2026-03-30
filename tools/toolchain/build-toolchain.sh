#!/usr/bin/env bash
# Build the shared ringos toolchain package for all targets and archive it as a distributable ZIP.

set -euo pipefail

if [[ $# -ge 1 ]]; then
  output_zip="$1"
else
  output_zip="$PWD/build/ringos-toolchain.zip"
fi

case "${output_zip}" in
  /*)
    output_zip_path="${output_zip}"
    ;;
  *)
    output_zip_path="$PWD/${output_zip}"
    ;;
esac

install_root="$(mktemp -d)"
staging_root="$(mktemp -d)"
package_root="${staging_root}/ringos-toolchain"

cleanup()
{
  rm -rf "${install_root}" "${staging_root}"
}

trap cleanup EXIT

mkdir -p "$(dirname "${output_zip_path}")"

cmake --preset x64-debug -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${install_root}"
cmake --build --preset build-x64-installed-toolchain

cmake --preset arm64-debug -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${install_root}"
cmake --build --preset build-arm64-installed-toolchain

mkdir -p "${package_root}"
cp -R "${install_root}/." "${package_root}/"

rm -f "${output_zip_path}"
(
  cd "${staging_root}"
  cmake -E tar cf "${output_zip_path}" --format=zip ringos-toolchain
)

echo "Built shared toolchain archive: ${output_zip_path}"
echo "x64 toolchain file in archive: ringos-toolchain/cmake/ringos-x64-toolchain.cmake"
echo "arm64 toolchain file in archive: ringos-toolchain/cmake/ringos-arm64-toolchain.cmake"
echo "generic toolchain file in archive: ringos-toolchain/cmake/ringos-toolchain.cmake"
