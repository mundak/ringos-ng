# Debug Helpers

This directory contains reusable helpers for debugging RingOS kernels under QEMU's GDB stub from Docker.

## Image

Build the base sample-test image first:

```powershell
docker build -f tests/tests.Dockerfile -t ringos-sample-tests .
```

Build the debug image with GDB tooling:

```powershell
docker build -f debug/Dockerfile -t ringos-debug-tools .
```

## Default Usage

Run the generic GDB-stub helper against the local toolchain archive in `build/`:

```powershell
docker run --rm -v "${PWD}:/workspace-host:ro" ringos-debug-tools bash /workspace-host/debug/gdb-stub.sh --arch x64
docker run --rm -v "${PWD}:/workspace-host:ro" ringos-debug-tools bash /workspace-host/debug/gdb-stub.sh --arch arm64
```

The script:

- copies the repo into a clean Linux workspace
- extracts the newest local `ringos-toolchain-*.tar.xz`
- builds the default `hello_world` sample in `Debug` when a sample override is configured
- rebuilds the matching kernel target, either with that sample embedded or with the kernel target's built-in embedded app
- launches QEMU with `-S` and a GDB stub
- runs a bounded `gdb-multiarch` batch session against the architecture-specific syscall handler

## Common Options

```text
--arch <x64|arm64>
--sample-project <path inside /repo>
--sample-target <cmake target>
--kernel-target <cmake target>
--test-app-binary-var <cmake cache variable>
--breakpoint <symbol>
--hit-count <count>
--gdb-port <port>
--toolchain-archive-dir <path>
```

For the default hello-world flow, `--test-app-binary-var` is inferred automatically.
If you omit that option for another kernel target, the script debugs the kernel target's built-in embedded app.
Pass `--test-app-binary-var` only when you need to override the sample binary that gets embedded.

## Examples

Use a different GDB port:

```powershell
docker run --rm -v "${PWD}:/workspace-host:ro" ringos-debug-tools bash /workspace-host/debug/gdb-stub.sh --arch x64 --gdb-port 2345
```

Debug the arm64 hello world C++ target by selecting a different kernel target and breakpoint hit limit:

```powershell
docker run --rm -v "${PWD}:/workspace-host:ro" ringos-debug-tools bash /workspace-host/debug/gdb-stub.sh --arch arm64 --kernel-target ringos_arm64_hello_world_cpp --hit-count 4
```

Point the helper at a different toolchain archive directory:

```powershell
docker run --rm -v "${PWD}:/workspace-host:ro" ringos-debug-tools bash /workspace-host/debug/gdb-stub.sh --arch x64 --toolchain-archive-dir /workspace-host/out
```

## Notes

- x64 uses the saved `.elf64` symbol file while booting the Multiboot-compatible converted kernel image.
- arm64 keeps semihost logging enabled with `target=native`, so host-visible debug lines still land in the captured QEMU log while GDB is attached.
- Set `RINGOS_QEMU_BIN` inside the container if you need to replace the QEMU binary without editing the script.
