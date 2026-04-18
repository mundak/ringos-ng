# Toolchain Workflow

This directory contains the local workflow for building and iterating on the
RingOS LLVM-based toolchain.

## Local Docker Build

Build the full local toolchain package with the standard Windows wrapper:

```bat
tools\toolchain\docker-build-toolchain.bat
```

That wrapper builds the shared toolchain image, mounts the persistent Docker
volume `ringos-toolchain-build` at `/workspace/build`, reuses the cached LLVM
source, build, and install trees across runs.

## Iterating on Cached LLVM Sources

When you need to debug or patch LLVM itself, edit the unpacked source tree in
the mounted Docker volume directly instead of rebuilding from a fresh archive on
every attempt.

Inspect the cached volume layout:

```powershell
docker volume inspect ringos-toolchain-build
docker run --rm -v "ringos-toolchain-build:/cache" ubuntu:24.04 bash -lc "find /cache/toolchain-build/bootstrap-llvm -maxdepth 3 -type d | sort"
```

The pinned LLVM source tree lives under a versioned path like:

```text
/cache/toolchain-build/bootstrap-llvm/src/llvm-project-<llvm-ref>
```

For example, the x64 RingOS SEH crash was reproduced and patched by editing:

```text
/cache/toolchain-build/bootstrap-llvm/src/llvm-project-<llvm-ref>/llvm/lib/Target/X86/X86MCInstLower.cpp
```

You can mount both the Docker volume and the workspace and run helper scripts
from the repo against the cached source tree:

```powershell
docker run --rm `
  -v "ringos-toolchain-build:/cache" `
  -v "${PWD}:/workspace" `
  ubuntu:24.04 `
  bash /workspace/tools/toolchain/<helper>.sh
```

After each source edit, rerun the normal wrapper:

```bat
tools\toolchain\docker-build-toolchain.bat
```

Because `tools/toolchain/build-toolchain.sh` reuses an existing
`bootstrap-llvm/src/llvm-project-*` tree when it is already present, the next
wrapper run picks up your in-volume source edit directly.

## Capturing a Permanent Patch

Once the cached LLVM edit is validated, persist it under `tools/llvm/patches/`
as a numbered patch so fresh toolchain builds apply it automatically.

The expected workflow is:

1. Reproduce the issue using `tools\toolchain\docker-build-toolchain.bat`.
2. Edit the cached source in `ringos-toolchain-build`.
3. Re-run `tools\toolchain\docker-build-toolchain.bat` until the LLVM-side
   issue is fixed.
4. Convert the validated edit into a new numbered patch under
   `tools/llvm/patches/`.
5. Rebuild again from the normal wrapper so future fresh caches pick up the
   checked-in patch automatically.

For step 4, do not hand-edit unified diff hunks. The more reliable process is:

1. Copy the edited cached file into a temporary `after` file.
2. Create a matching `before` file by reverting only the intended code change.
3. Generate the patch text with `git diff --no-index` from those two files.
4. Rewrite only the patch headers (`diff --git`, `---`, `+++`) to the real
   upstream LLVM path before checking the patch in.
5. Keep the patch file LF-only. CRLF line endings can make `git apply` fail in
   the Linux container even when the patch text is otherwise correct.
6. Validate the checked-in patch against the cached source tree before relying
   on it:

```powershell
docker run --rm `
  -v "ringos-toolchain-build:/cache" `
  -v "${PWD}:/workspace" `
  ringos-ci-toolchain-release `
  bash -lc "cd /cache/toolchain-build/bootstrap-llvm/src/llvm-project-<llvm-ref> && git apply --reverse --check /workspace/tools/llvm/patches/000N-description.patch"
```

Generating the hunk from the real file pair and validating it with `git apply`
is more reliable than writing hunk headers or line counts by hand.

## Current Status

The x64 native RingOS clang SEH assertion in
`llvm/lib/Target/X86/X86MCInstLower.cpp` was fixed successfully with this
workflow, and the local `tools\toolchain\docker-build-toolchain.bat` flow now
completes end to end again.

The packaging path is now isolated from the main repo build graph: the release
scripts assemble the published bundle layout directly, the toolchain runtime
payload is built through the standalone `user/libc/CMakeLists.txt` and
`user/crt/CMakeLists.txt` projects, and the SDK bundle is built separately
through `user/sdk/CMakeLists.txt` instead of the repo root. That keeps
`arch/*`, `kernel/`, `win32/tests/`, and the embedded `*_test_app_image.cmake`
helpers out of the release packaging path.

One practical cache note remains: if you change fundamental SDK payload
configure inputs during manual iteration, delete `build/sdk-build/x64` and
`build/sdk-build/arm64` before re-running the configure step. The standard SDK
wrapper now does this automatically.
