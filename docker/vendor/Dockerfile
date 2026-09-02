# ------------------------------
# Build stage - builds webrtc, mediasoupclient, sdptransform, vendors onnxruntime
# ------------------------------
FROM debian:bookworm AS builder

WORKDIR /app

# Install build dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    ninja-build \
    clang \
    python3 \
    curl \
    wget \
    tar \
    xz-utils \
    pkg-config \
    libx11-dev \
    libpulse-dev \
    libasound2-dev \
    libstdc++-12-dev \
    binutils \
    lsb-release \
    sudo \
    && rm -rf /var/lib/apt/lists/*

# Copy vendor scripts and patches
COPY scripts/ ./scripts/
COPY patches/ ./patches/

# Build everything (non-interactive mode for Docker)
RUN ./scripts/vendor-webrtc-deps-linux.sh docker

# ------------------------------
# Final stage - just the vendored libs (~200-300MB)
# ------------------------------
FROM debian:bookworm-slim

WORKDIR /prebuilt

# Copy vendored libraries and headers
COPY --from=builder /app/third_party/prebuilt/linux-x64/ ./

# Verify artifacts exist
RUN ls -la lib/ && ls -la include/
