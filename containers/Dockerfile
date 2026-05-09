# ============================================================
# GRANITE Development Container
#
# Includes all dependencies for building and testing GRANITE.
# Designed for development; for HPC, use the Singularity recipe.
#
# Build:  docker build -t granite:dev .
# Run:    docker run -it --rm -v $(pwd):/workspace granite:dev
# Test:   docker run --rm granite:dev bash -c "cd /build && ctest"
# ============================================================

FROM ubuntu:24.04 AS base

LABEL maintainer="Liran M. Schwartz <scliran9@gmail.com>"
LABEL description="GRANITE NR Development Environment"

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# ── System packages ──
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    g++-13 \
    clang-17 \
    libopenmpi-dev \
    openmpi-bin \
    libhdf5-openmpi-dev \
    libpetsc-dev \
    python3 \
    python3-pip \
    python3-numpy \
    python3-scipy \
    python3-matplotlib \
    python3-h5py \
    doxygen \
    graphviz \
    cppcheck \
    valgrind \
    gdb \
    curl \
    wget \
    && rm -rf /var/lib/apt/lists/*

# ── Python packages for analysis ──
RUN pip3 install --break-system-packages \
    sphinx \
    breathe \
    sphinx-rtd-theme \
    pytest \
    black \
    jupyter

# ── Build GRANITE ──
WORKDIR /granite
COPY . /granite

RUN cmake -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DGRANITE_ENABLE_MPI=ON \
    -DGRANITE_ENABLE_GPU=OFF \
    -DGRANITE_ENABLE_TESTS=ON \
    -DGRANITE_ENABLE_HDF5=ON \
    && cmake --build /build -j$(nproc)

# ── Default: run tests ──
CMD ["bash", "-c", "cd /build && ctest --output-on-failure"]
