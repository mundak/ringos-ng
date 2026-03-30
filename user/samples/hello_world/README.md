# Hello World Sample

This sample is intended to build only against the published ringos installed-toolchain bundle.
Do not point it at repo-local `build/<preset>/sysroot` trees or an arbitrary host `clang`.

## Resolve The Toolchain Bundle

From the repository root, download the matching toolchain bundle from GitHub Releases into the default cache location:

```bash
bash tools/toolchain/ensure-toolchain-release.sh --repo mundak/ringos-ng
```

For private repositories, export `GH_TOKEN` or `GITHUB_TOKEN` first so the helper can authenticate the download.

## Configure And Build

Linux or macOS:

```bash
cmake -S user/samples/hello_world -B build/user-samples/hello_world-x64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/.cache/ringos/toolchain/cmake/ringos-toolchain.cmake" \
  -DRINGOS_TARGET_ARCH=x64
cmake --build build/user-samples/hello_world-x64
```

Windows PowerShell:

```powershell
cmake -S user/samples/hello_world -B build/user-samples/hello_world-x64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:LOCALAPPDATA/ringos/toolchain/cmake/ringos-toolchain.cmake" `
  -DRINGOS_TARGET_ARCH=x64
cmake --build build/user-samples/hello_world-x64
```

Swap `x64` for `arm64` in both the build directory name and `RINGOS_TARGET_ARCH` when you want the arm64 sample output.
