FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
  ca-certificates \
  cmake \
  curl \
  dos2unix \
  ninja-build \
  python3 \
  qemu-system-arm \
  qemu-system-misc \
  qemu-system-x86 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY . .

RUN find /workspace -type f -name '*.sh' -exec dos2unix {} + \
  && find /workspace -type f -name '*.sh' -exec chmod +x {} +

CMD ["bash"]
