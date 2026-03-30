# Stage 8: Clang Bring-Up

This document freezes the first implementation contract for Stage 8.

Stage 7 proved that ringos-ng can package a hosted C sysroot and compile user
programs through external Clang invocations that rely on bootstrap config
files. Stage 8 moves that knowledge into the compiler so ringos becomes a named
target rather than an ad hoc cross-build recipe.

## Target Contract

Stage 8 adopts these compiler-facing target triples:

- `x86_64-unknown-ringos-msvc`
- `aarch64-unknown-ringos-msvc`

The `ringos` OS name is new in Stage 8. The `msvc` environment stays in place
for the bootstrap cut because the current user image format, archive naming,
and linker flow are all PE or COFF based.

Stage 8 does **not** introduce a new object format, a new loader contract, or a
new kernel ABI. The ringos-aware compiler should keep using:

- PE32+ user images for x64 and arm64
- `lld-link` as the default linker
- the existing fixed-base bootstrap image policy until the loader grows beyond
  the current proof path

## Sysroot Contract

The staged sysroot layout remains the source of truth:

- `user/sysroot/sysroot/<triple>/include` for headers
- `user/sysroot/sysroot/<triple>/lib` for `crt0.obj`, `ringos_c.lib`, `ringos_sdk.lib`, and
  `clang_rt.builtins.lib`
- `user/sysroot/sysroot/<triple>/share/ringos` for metadata

The generated compiler should accept an explicit `--sysroot`, but the normal
hosted path should also work through a compiler-relative default sysroot.

The bootstrap config files under `share/ringos` remain available during the
transition, but the Stage 8 exit path is that a ringos-aware compiler no longer
requires them for the common hosted C flow.

## Driver Defaults

For the first Stage 8 implementation, the ringos Clang driver should:

- recognize the ringos target triples without custom wrapper scripts
- select `lld-link`
- search the ringos sysroot for headers and libraries by default
- inject `crt0.obj`, `ringos_c.lib`, `ringos_sdk.lib`, and
  `clang_rt.builtins.lib` in the same order used by the current bootstrap link
  config
- preserve the existing image-base and file-alignment policy until a later
  loader stage changes it deliberately

This is driver work, not backend work. Code generation should continue to use
the existing x64 and arm64 LLVM targets.

## Repository-Owned Build Flow

The repository now carries two bootstrap scripts for this stage:

- `tools/llvm/build-clang-toolchain.sh` builds a pinned `llvm-project` checkout,
  applies any in-repo ringos patch set from `tools/llvm/patches`, and installs
  host tools under `user/sysroot`
- `scripts/build-bootstrap-hosted-c.sh` proves the current hosted C sysroot path
  by building the staged sysroot and compiling a user-space C sample against it

The first script starts the LLVM and Clang bring-up lane. The second script is
the transition harness that the future ringos-aware compiler should replace.

## Initial Implementation Order

1. Freeze the target and sysroot contract in repo docs.
2. Build host LLVM, Clang, LLD, and compiler-rt from a pinned source revision.
3. Add ringos triple and driver support in `llvm-project` patches.
4. Replace bootstrap config-file use in the hosted C flow with direct target
   support from the generated compiler.
5. Promote that flow into CI once the direct hosted C path is stable on both
   architectures.

## Non-Goals

Stage 8 does not try to solve:

- hosted C++ runtime support
- dynamic linking or PIE
- Windows personality support
- kernel ABI redesign
- moving away from PE or COFF in the bootstrap environment
