# Image tags - required, no defaults
ARG VENDOR_TAG
ARG BUILDER_DEPS_TAG

# ------------------------------
# Vendor stage (for COPY --from)
# ------------------------------
FROM ghcr.io/saasybyte/saasy-media-engine-vendor:${VENDOR_TAG} AS vendor

# ------------------------------
# Builder-deps stage (for FROM)
# ------------------------------
FROM ghcr.io/saasybyte/saasy-media-engine-builder-deps:${BUILDER_DEPS_TAG} AS builder-deps

# ------------------------------
# Build stage
# ------------------------------
FROM builder-deps AS builder

# Copy prebuilt libs from vendor image
COPY --from=vendor /prebuilt ./third_party/prebuilt/linux-x64

# Copy source code (submodule must be initialized before docker build)
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY common/ ./common/
COPY speaking-engine/ ./speaking-engine/
COPY listening-engine/ ./listening-engine/
COPY third_party/saasy-proto/ ./third_party/saasy-proto/

# Configure CMake
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_OVERLAY_TRIPLETS=./cmake/triplets \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release \
    -DVCPKG_HOST_TRIPLET=x64-linux-release \
    -DVCPKG_INSTALLED_DIR=/vcpkg/installed \
    -DCMAKE_BUILD_TYPE=Release

# Build listening-engine
RUN cmake --build build --target listening-engine

# ------------------------------
# Runtime stage
# ------------------------------
FROM debian:bookworm-slim

WORKDIR /app

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libx11-6 \
    libstdc++6 \
    libatomic1 \
    && rm -rf /var/lib/apt/lists/*

# Add grpc-health-probe for healthchecks
ADD https://github.com/grpc-ecosystem/grpc-health-probe/releases/download/v0.4.35/grpc_health_probe-linux-amd64 \
    /usr/local/bin/grpc_health_probe
RUN chmod +x /usr/local/bin/grpc_health_probe

# Copy binary from builder
COPY --from=builder /app/build/listening-engine/listening-engine ./bin/listening-engine

# Copy ONNX Runtime shared libraries
COPY --from=builder /app/third_party/prebuilt/linux-x64/lib/libonnxruntime.so* /usr/local/lib/
COPY --from=builder /app/third_party/prebuilt/linux-x64/lib/libonnxruntime_providers_shared.so /usr/local/lib/

# Copy ONNX models
COPY listening-engine/models/ ./listening-engine/models/

# Copy mel filter assets
COPY listening-engine/assets/ ./listening-engine/assets/

# Set library path for ONNX Runtime
ENV LD_LIBRARY_PATH=/usr/local/lib

# Update linker cache
RUN ldconfig

# UDS socket will be at /tmp/listening-engine.sock
# This is configurable at runtime

# Run the application
CMD ["./bin/listening-engine"]
