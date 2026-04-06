FROM ringos-sample-tests:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
  binutils \
  gdb-multiarch \
  tar \
  && rm -rf /var/lib/apt/lists/*