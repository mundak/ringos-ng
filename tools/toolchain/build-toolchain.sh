#!/usr/bin/env bash
# Build the shared ringos toolchain package for all targets and archive it as a distributable tar.xz.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

usage()
{
  cat <<EOF
Usage: tools/toolchain/build-toolchain.sh [options] [output-archive]

Options:
  --output <path>          Output archive path.
  --version <version>      Toolchain version recorded in the bundle metadata.
  --help                   Show this help text.
EOF
}

output_archive=""
toolchain_version="${RINGOS_TOOLCHAIN_VERSION:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output)
      output_archive="$2"
      shift 2
      ;;
    --version)
      toolchain_version="$2"
      shift 2
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

if [[ -z "${toolchain_version}" ]]; then
  toolchain_version="dev-local"
fi

external_previous_stage_root="${RINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT:-}"
bootstrap_previous_stage_root=0

install_root="$(mktemp -d)"
staging_root="$(mktemp -d)"
package_root="${staging_root}/ringos-toolchain"
x64_build_dir="${staging_root}/build-x64"
arm64_build_dir="${staging_root}/build-arm64"

if [[ -n "${external_previous_stage_root}" ]]; then
  previous_stage_root="${external_previous_stage_root}"

  if [[ ! -x "${previous_stage_root}/bin/clang" ]]; then
    echo "Configured previous-stage toolchain root does not contain bin/clang: ${previous_stage_root}" >&2
    exit 1
  fi
else
  previous_stage_root="$(mktemp -d)"
  bootstrap_previous_stage_root=1
  RINGOS_LLVM_INSTALL_DIR="${previous_stage_root}" bash "${repo_root}/tools/llvm/build-clang-toolchain.sh"
fi

cleanup()
{
  rm -rf "${install_root}" "${staging_root}"

  if [[ "${bootstrap_previous_stage_root}" == "1" ]]; then
    rm -rf "${previous_stage_root}"
  fi
}

trap cleanup EXIT

validate_packaged_toolchain()
{
  local bundle_root="$1"
  local -a expected_tools=(clang clang++ lld-link llvm-ar llvm-ranlib llvm-objcopy)
  local -a expected_target_triples=(x86_64-unknown-ringos-msvc aarch64-unknown-ringos-msvc)
  local tool_name=""
  local target_triple=""

  for tool_name in "${expected_tools[@]}"; do
    if [[ ! -e "${bundle_root}/bin/${tool_name}" ]]; then
      echo "Packaged toolchain bundle is missing bin/${tool_name}." >&2
      exit 1
    fi
  done

  for target_triple in "${expected_target_triples[@]}"; do
    if [[ ! -e "${bundle_root}/sysroots/${target_triple}/include/c++/v1/__config" ]]; then
      echo "Packaged toolchain bundle is missing libc++ headers for ${target_triple}." >&2
      exit 1
    fi
  done
}

cmake -S "${repo_root}" \
  -B "${x64_build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DRINGOS_ENABLE_TESTING=OFF \
  -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/toolchains/x64.cmake" \
  -DRINGOS_TARGET_ARCH=x64 \
  -DRINGOS_TOOLCHAIN_VERSION="${toolchain_version}" \
  -DRINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT="${previous_stage_root}" \
  -DRINGOS_TOOLCHAIN_DRIVER_MODE=ringos-native \
  -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${install_root}"
cmake --build "${x64_build_dir}" --target ringos_installed_toolchain

cmake -S "${repo_root}" \
  -B "${arm64_build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DRINGOS_ENABLE_TESTING=OFF \
  -DCMAKE_TOOLCHAIN_FILE="${repo_root}/cmake/toolchains/arm64.cmake" \
  -DRINGOS_TARGET_ARCH=arm64 \
  -DRINGOS_TOOLCHAIN_VERSION="${toolchain_version}" \
  -DRINGOS_PREVIOUS_STAGE_TOOLCHAIN_ROOT="${previous_stage_root}" \
  -DRINGOS_TOOLCHAIN_DRIVER_MODE=ringos-native \
  -DRINGOS_TOOLCHAIN_INSTALL_ROOT="${install_root}"
cmake --build "${arm64_build_dir}" --target ringos_installed_toolchain

toolchain_version_file="${install_root}/share/ringos/toolchain-version.txt"
printf '%s\n' "${toolchain_version}" > "${toolchain_version_file}"

archive_stem="ringos-toolchain-${toolchain_version}"

if [[ -z "${output_archive}" ]]; then
  output_archive="${repo_root}/build/${archive_stem}.tar.xz"
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

mkdir -p "${package_root}"
cp -a "${install_root}/." "${package_root}/"
validate_packaged_toolchain "${package_root}"

rm -f "${output_archive_path}"
(
  cd "${staging_root}"
  cmake -E tar cJf "${output_archive_path}" ringos-toolchain
)

echo "Built shared toolchain archive: ${output_archive_path}"
echo "Toolchain version: ${toolchain_version}"
echo "x64 toolchain file in archive: ringos-toolchain/cmake/ringos-x64-toolchain.cmake"
echo "arm64 toolchain file in archive: ringos-toolchain/cmake/ringos-arm64-toolchain.cmake"
echo "generic toolchain file in archive: ringos-toolchain/cmake/ringos-toolchain.cmake"
