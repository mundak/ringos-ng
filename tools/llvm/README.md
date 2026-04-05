# RingOS llvm-project patch set

This directory holds the Stage 8 llvm-project patch series that `tools/toolchain/build-toolchain.sh` applies on top of the exact llvm-project commit `3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff` (`llvmorg-18.1.8`).

Default bootstrap layout:

- `build/toolchain-build/bootstrap-llvm/src/llvm-project-<llvm-ref>` for the pinned upstream source tree extracted from the downloaded archive name
- `build/toolchain-build/bootstrap-llvm/build-<llvm-ref>` for the CMake and Ninja build tree paired with that pinned source tree
- `build/toolchain-build/bootstrap-llvm/install` for the installed Clang and lld bootstrap
- `build/toolchain-build/bootstrap-llvm/downloads` for the pinned source archives fetched from GitHub
- `tools/llvm/patches` for the in-repo RingOS patch series

The full bootstrap path in `tools/toolchain/build-toolchain.sh` downloads the
pinned upstream source archive, builds the patched Clang and lld bootstrap,
stages libc++ headers and the rest of the target sysroots, and then bundles the
result into the versioned toolchain archive.

That build script fetches only that exact pinned revision by default rather than
refreshing against moving tags or the latest upstream branch state.

If the versioned source directory for the downloaded archive already exists, the
script assumes that tree was already downloaded, extracted, and patched, and it
skips those preparation steps on subsequent runs.

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

When updating these patches, regenerate them from a clean checkout of commit `3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff` (`llvmorg-18.1.8`) rather than editing patch hunks by hand.
