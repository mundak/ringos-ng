# RingOS llvm-project patch set

This directory holds the Stage 8 llvm-project patch series and bootstrap script that `tools/llvm/build-clang-toolchain.sh` applies on top of `llvmorg-18.1.8`.

Default layout:

- `tools/llvm/src/llvm-project` for the pinned upstream checkout
- `tools/llvm/build` for the CMake and Ninja build tree
- `tools/llvm/patches` for the in-repo RingOS patch series
- `user/sysroot` for the installed host tools and the compiler-relative RingOS sysroot

Bootstrap hosted-C++ bundles also use the pinned upstream checkout as the source
of libc++ headers. Run `tools/llvm/ensure-libcxx-source.sh` when you need only
the libc++ header tree under `tools/llvm/src/llvm-project/libcxx/include`
without building the full Clang toolchain.

Patch order:

1. `patches/0001-llvm-add-ringos-triple.patch`
   Adds `ringos` to LLVM Triple parsing, printing, and unit tests.

2. `patches/0002-clang-add-ringos-toolchain.patch`
   Adds an initial Clang RingOS driver/toolchain that:
   - selects a dedicated `RingOSToolChain` for `*-unknown-ringos-msvc`
   - uses `lld-link`
   - defaults to `/entry:user_start` and `/subsystem:native`
   - consumes the RingOS sysroot layout under `<triple>/include` and `<triple>/lib`

Current limitations of the initial driver patch:

- It is intentionally scoped to the hosted C bootstrap path.
- The installed compiler expects RingOS sysroots under `user/sysroot/sysroot/<triple>/` by default.
- Shared-library, import-library, and broader MSVC compatibility behaviors are not implemented yet.
- Compiler-rt naming is still repo-specific via `clang_rt.builtins.lib`; this series does not yet normalize RingOS runtime naming inside Clang.

When updating these patches, regenerate them from a clean `llvmorg-18.1.8` checkout rather than editing patch hunks by hand.
