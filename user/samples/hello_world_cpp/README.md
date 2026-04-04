# Hello World C++ Sample

This sample is intended to build only against the published ringos installed-toolchain bundle.
Do not point it at repo-local `build/<preset>/sysroot` trees or an arbitrary host `clang++`.

The sample now exercises the first hosted-C++ bootstrap path:

- C++20 translation units compiled with the bundled `clang++`
- upstream libc++ headers staged under the installed sysroot at `include/c++/v1`
- PE and COFF startup that runs `.CRT$X*` initializers before `main()` and `atexit()` handlers during `exit()`
- a currently validated header subset centered on libc++ core template headers such as `type_traits`
- the current bootstrap constraints of `-fno-exceptions`, `-fno-rtti`, and `-fno-threadsafe-statics`

## Resolve The Toolchain Bundle

From the repository root, download the latest published toolchain bundle from GitHub Releases into the default cache location:

```bash
bash tools/toolchain/ensure-toolchain-release.sh --repo mundak/ringos-ng
```

For private repositories, export `GH_TOKEN` or `GITHUB_TOKEN` first so the helper can authenticate the download.

If you are rebuilding the toolchain bundle locally rather than downloading a published release,
populate the pinned LLVM libc++ headers first:

```bash
bash tools/llvm/ensure-libcxx-source.sh
```

## Configure And Build

Linux or macOS:

```bash
cmake -S user/samples/hello_world_cpp -B build/user-samples/hello_world_cpp-x64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/.cache/ringos/toolchain/cmake/ringos-toolchain.cmake" \
  -DRINGOS_TARGET_ARCH=x64
cmake --build build/user-samples/hello_world_cpp-x64
```

Windows PowerShell:

```powershell
cmake -S user/samples/hello_world_cpp -B build/user-samples/hello_world_cpp-x64 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:LOCALAPPDATA/ringos/toolchain/cmake/ringos-toolchain.cmake" `
  -DRINGOS_TARGET_ARCH=x64
cmake --build build/user-samples/hello_world_cpp-x64
```

Swap `x64` for `arm64` in both the build directory name and `RINGOS_TARGET_ARCH` when you want the arm64 sample output.

For a direct bundled-compiler invocation instead of CMake, use:

```bash
bash scripts/build-bootstrap-hosted-cpp.sh x64 user/samples/hello_world_cpp/hello_world.cpp
bash scripts/build-bootstrap-hosted-cpp.sh arm64 user/samples/hello_world_cpp/hello_world.cpp
```

## What The Toolchain Still Needs

The installed bundle can now compile and link simple hosted C++ entry points against staged libc++ headers, but broader C++ support still needs more runtime surface:

- `operator new` and `operator delete` implementations for heap-backed C++ allocation
- staged `libc++.lib`, `libc++abi.lib`, and unwind support once the target libc surface is large enough for those libraries
- a real user-space allocator so containers and owning string types can allocate storage
- enough C-library surface for libc++ compatibility wrappers such as `cstdio`, `cstdlib`, `cstring`, and `cwchar`
- MSVC-compatible thread-safe static initialization hooks (`_Init_thread_header`, `_Init_thread_footer`, `_Init_thread_epoch`, and TLS support)
- exception and RTTI support once the ABI runtime and unwind story are in place
