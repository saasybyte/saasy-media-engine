# ------------------------------
# Builder dependencies image
# Contains: apt build deps + vcpkg deps (gRPC, protobuf, boost, spdlog)
# Rebuild when: vcpkg.json changes
# ------------------------------
FROM debian:bookworm

WORKDIR /app

# Install build dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    ninja-build \
    clang \
    pkg-config \
    libx11-dev \
    libstdc++-12-dev \
    curl \
    zip \
    unzip \
    tar \
    && rm -rf /var/lib/apt/lists/*

# Clone and bootstrap vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /vcpkg \
    && /vcpkg/bootstrap-vcpkg.sh

# Copy vcpkg manifest and triplets
COPY vcpkg.json ./
COPY cmake/triplets/ ./cmake/triplets/

# Set compiler BEFORE vcpkg install
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

# Install vcpkg dependencies (~11 min, cached in this image)
RUN /vcpkg/vcpkg install \
    --triplet x64-linux-release \
    --host-triplet x64-linux-release \
    --overlay-triplets=./cmake/triplets
