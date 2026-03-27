# Shared Docker image for the repository CI toolchain.
# Mirrors the dependency stack from .github/workflows/ci.yml so both
# architectures build and test in the same environment.

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
  cmake \
  ninja-build \
  clang \
  lld \
  llvm \
  qemu-system-arm \
  qemu-system-misc \
  qemu-system-x86 \
  dos2unix \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY . .

# Fix Windows line endings so shell scripts run correctly in Linux.
RUN find /workspace/scripts -type f -name '*.sh' -exec dos2unix {} +

CMD ["bash"]
