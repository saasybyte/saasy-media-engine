#!/bin/bash

# Detect platform
if [[ "$(uname -s)" == "Darwin" ]]; then
    TRIPLET="arm64-osx-release"
    COMPILER="/usr/bin/clang++"
else
    TRIPLET="x64-linux-release"
    COMPILER="/usr/bin/clang++"
fi

# Ensure vcpkg is available
if [ ! -d "vcpkg" ]; then
    echo "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
fi

cmake -S . -B build -G Ninja \
    -D CMAKE_CXX_COMPILER="$COMPILER" \
    -D CMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
    -D VCPKG_OVERLAY_TRIPLETS=./cmake/triplets \
    -D VCPKG_TARGET_TRIPLET="$TRIPLET" \
    -D VCPKG_HOST_TRIPLET="$TRIPLET" \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -D CMAKE_POLICY_VERSION_MINIMUM=3.5