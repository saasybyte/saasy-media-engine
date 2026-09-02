#!/bin/bash
set -euo pipefail

# =======================================================================================
# This script builds libwebrtc and libmediasoupclient from source for macOS arm64
# and vendors the required static libraries and headers into the repository
# for consistent builds.
#
# Following: https://mediasoup.org/documentation/v3/libmediasoupclient/installation/
#
# IMPORTANT: On macOS, this script also prefixes all BoringSSL symbols with BSSL_
# to avoid conflicts with OpenSSL (used by gRPC via vcpkg). Linux uses
# --allow-multiple-definition linker flag instead.
# =======================================================================================

readonly SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd "${SCRIPTS_DIR}/.." && pwd)"
readonly VENDOR_CACHE_DIR="${PROJECT_ROOT}/../vendor-cache"

# Output Colors
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly NC='\033[0m' # No Color

# Configuration
readonly BUILD_DIR="${PROJECT_ROOT}/vendor-build"
readonly VENDOR_DIR="${PROJECT_ROOT}/third_party/prebuilt"
readonly DEPOT_TOOLS_DIR="${BUILD_DIR}/depot_tools"
readonly NUM_JOBS=$(sysctl -n hw.ncpu)

# Versions
readonly LIBWEBRTC_BRANCH="m140"
readonly LIBWEBRTC_COMMIT="7339"
readonly LIBMEDIASOUPCLIENT_VERSION="3.5.0"

# Cache file names (versioned)
readonly WEBRTC_CACHE_FILE="webrtc-checkout-${LIBWEBRTC_BRANCH}-${LIBWEBRTC_COMMIT}-darwin.tar.gz"
readonly MEDIASOUP_CACHE_FILE="libmediasoupclient-${LIBMEDIASOUPCLIENT_VERSION}.tar.gz"

# BoringSSL prefix (used to avoid OpenSSL symbol conflicts on macOS)
readonly BORINGSSL_PREFIX="BSSL"

# Platform
readonly PLATFORM="darwin-arm64"
readonly PLATFORM_VENDOR_DIR="${VENDOR_DIR}/${PLATFORM}"

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Initialize vendor cache directory
init_vendor_cache() {
    if [ ! -d "${VENDOR_CACHE_DIR}" ]; then
        log_info "Creating vendor cache directory: ${VENDOR_CACHE_DIR}"
        mkdir -p "${VENDOR_CACHE_DIR}"
    fi
}

# Check if cached file exists
cache_exists() {
    local cache_file="$1"
    [ -f "${VENDOR_CACHE_DIR}/${cache_file}" ]
}

# Extract from cache
extract_from_cache() {
    local cache_file="$1"
    local dest_dir="$2"
    
    log_info "Extracting from cache: ${cache_file}"
    mkdir -p "${dest_dir}"
    tar -xzf "${VENDOR_CACHE_DIR}/${cache_file}" -C "${dest_dir}"
    log_info "Cache extraction complete"
}

# Save to cache
save_to_cache() {
    local source_dir="$1"
    local cache_file="$2"
    
    log_info "Saving to cache: ${cache_file} (this may take a few minutes)..."
    tar -czf "${VENDOR_CACHE_DIR}/${cache_file}" -C "$(dirname "${source_dir}")" "$(basename "${source_dir}")"
    local size=$(du -h "${VENDOR_CACHE_DIR}/${cache_file}" | cut -f1)
    log_info "Cached ${cache_file} (${size})"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    local missing_deps=()
    
    # Check required tools
    command -v git >/dev/null 2>&1 || missing_deps+=("git")
    command -v cmake >/dev/null 2>&1 || missing_deps+=("cmake")
    command -v python3 >/dev/null 2>&1 || missing_deps+=("python3")
    command -v clang >/dev/null 2>&1 || missing_deps+=("clang (install Xcode CLI tools)")
    command -v ninja >/dev/null 2>&1 || missing_deps+=("ninja")
    command -v go >/dev/null 2>&1 || missing_deps+=("go")
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Install with: brew install ${missing_deps[*]}"
        exit 1
    fi
    
    # Check for Xcode CLI tools
    if ! xcode-select -p &>/dev/null; then
        log_error "Xcode Command Line Tools not installed"
        log_info "Install with: xcode-select --install"
        exit 1
    fi
    
    # Check disk space (need ~50GB for WebRTC build)
    local available_gb=$(df -g "${PROJECT_ROOT}" | awk 'NR==2 {print $4}')
    if [ "$available_gb" -lt 50 ]; then
        log_warn "Low disk space: ${available_gb}GB available (50GB recommended)"
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

setup_build_env() {
    log_info "Setting up build environment..."
    
    # Initialize vendor cache
    init_vendor_cache
    
    # Create directories
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${VENDOR_DIR}"
    
    # Setup depot_tools if not present
    if [ ! -d "${DEPOT_TOOLS_DIR}" ]; then
        log_info "Cloning depot_tools..."
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "${DEPOT_TOOLS_DIR}"
    fi

    # Add depot_tools to PATH
    export PATH="${DEPOT_TOOLS_DIR}:${PATH}"

    # Ensure depot_tools is bootstrapped (downloads tools on first run, may take a minute)
    log_info "Initializing depot_tools..."
    gclient --version >/dev/null 2>&1 || true
}

# Clean previous builds
clean_build() {
    if [ -d "${BUILD_DIR}" ]; then
        log_warn "Previous build artifacts found"
        read -p "Clean previous builds? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            log_info "Cleaning previous builds..."
            # Clean everything except depot_tools (reusable)
            find "${BUILD_DIR}" -mindepth 1 -maxdepth 1 ! -name "depot_tools" -exec rm -rf {} +
        fi
    fi
}

# Generate BoringSSL prefix headers
generate_boringssl_prefix() {
    local webrtc_src="$1"
    local boringssl_src="${webrtc_src}/third_party/boringssl/src"
    
    log_info "Generating BoringSSL prefix headers (prefix: ${BORINGSSL_PREFIX}_)..."
    
    # Build BoringSSL standalone with CMake to get clean archives
    log_info "Building BoringSSL standalone for symbol extraction..."
    mkdir -p "${boringssl_src}/build_prefix"
    cd "${boringssl_src}/build_prefix"
    cmake .. -GNinja
    ninja crypto ssl
    
    # Verify builds succeeded
    if [ ! -f "libssl.a" ] || [ ! -f "libcrypto.a" ]; then
        log_error "Failed to build BoringSSL standalone libraries"
        return 1
    fi
    
    # Generate symbol list using BoringSSL's Go scripts
    log_info "Extracting BoringSSL symbols..."
    cd "${boringssl_src}/util"
    go run read_symbols.go ../build_prefix/libssl.a > /tmp/boringssl_symbols.txt
    go run read_symbols.go ../build_prefix/libcrypto.a >> /tmp/boringssl_symbols.txt
    
    local symbol_count=$(wc -l < /tmp/boringssl_symbols.txt)
    log_info "Found ${symbol_count} BoringSSL symbols to prefix"
    
    # Generate prefix headers using CMake
    log_info "Generating prefix headers..."
    cd "${boringssl_src}/build_prefix"
    cmake .. -DBORINGSSL_PREFIX=${BORINGSSL_PREFIX} -DBORINGSSL_PREFIX_SYMBOLS=/tmp/boringssl_symbols.txt
    ninja boringssl_prefix_symbols
    
    # Verify headers were generated
    if [ ! -f "symbol_prefix_include/boringssl_prefix_symbols.h" ]; then
        log_error "Failed to generate boringssl_prefix_symbols.h"
        return 1
    fi
    
    # Copy prefix headers to include directory
    cp symbol_prefix_include/boringssl_prefix_symbols.h "${boringssl_src}/include/"
    cp symbol_prefix_include/boringssl_prefix_symbols_asm.h "${boringssl_src}/include/"
    
    log_info "BoringSSL prefix headers generated successfully"
}

# Patch BUILD.gn for BoringSSL symbol prefixing
patch_boringssl_buildgn() {
    local webrtc_src="$1"
    local build_gn="${webrtc_src}/third_party/boringssl/BUILD.gn"
    
    log_info "Patching BoringSSL BUILD.gn for symbol prefixing..."
    
    # Backup original if not already backed up
    if [ ! -f "${build_gn}.original" ]; then
        cp "${build_gn}" "${build_gn}.original"
    fi
    
    # Start fresh from original
    cp "${build_gn}.original" "${build_gn}"
    
    # Create a Python script to do the patching reliably
    python3 << EOF
import re

with open("${build_gn}", "r") as f:
    content = f.read()

# Patch internal_config: add BORINGSSL_PREFIX to defines
# Find: defines = [ "OPENSSL_SMALL" ]
# Replace with: defines = [ "OPENSSL_SMALL", "BORINGSSL_PREFIX=${BORINGSSL_PREFIX}" ]
content = re.sub(
    r'(config\("internal_config"\) \{[^}]*defines = \[\s*)"OPENSSL_SMALL"(\s*\])',
    r'\1"OPENSSL_SMALL",\n    "BORINGSSL_PREFIX=${BORINGSSL_PREFIX}",\2',
    content
)

# Patch external_config: add defines with BORINGSSL_PREFIX
# Find the external_config block and add defines after include_dirs
content = re.sub(
    r'(config\("external_config"\) \{[^}]*include_dirs = \[ "src/include" \])',
    r'\1\n  defines = [ "BORINGSSL_PREFIX=${BORINGSSL_PREFIX}" ]',
    content
)

# Patch boringssl_asm source_set: add defines for assembly files
# Find the else block with source_set("boringssl_asm") and add defines
content = re.sub(
    r'(source_set\("boringssl_asm"\) \{\s*\n\s*visibility = \[ ":\*" \]\s*\n\s*sources = rebase_path\(bcm_sources_asm \+ crypto_sources_asm, "\.", "src"\)\s*\n\s*include_dirs = \[ "src/include" \])\s*\n(\s*\})',
    r'\1\n    defines = [ "BORINGSSL_PREFIX=${BORINGSSL_PREFIX}" ]\n\2',
    content
)

with open("${build_gn}", "w") as f:
    f.write(content)

print("BUILD.gn patched successfully")
EOF

    log_info "BUILD.gn patched for BoringSSL symbol prefixing"
}

# Build libwebrtc
build_libwebrtc() {
    log_info "Building libwebrtc ${LIBWEBRTC_BRANCH}..."

    export PATH="${DEPOT_TOOLS_DIR}:${PATH}"

    local webrtc_dir="${BUILD_DIR}/webrtc-checkout"
    local webrtc_src="${webrtc_dir}/src"
    local webrtc_out="${webrtc_src}/out/${LIBWEBRTC_BRANCH}"
    
    # Step 1: Fetch WebRTC source (or restore from cache)
    if [ ! -d "${webrtc_src}" ]; then
        if cache_exists "${WEBRTC_CACHE_FILE}"; then
            log_info "Found cached WebRTC source, restoring..."
            extract_from_cache "${WEBRTC_CACHE_FILE}" "${BUILD_DIR}"
        else
            log_info "Fetching WebRTC source (this will take a while)..."
            mkdir -p "${webrtc_dir}"
            cd "${webrtc_dir}"
            
            fetch --nohooks webrtc
            gclient sync
            
            cd src
            git checkout -b ${LIBWEBRTC_BRANCH} refs/remotes/branch-heads/${LIBWEBRTC_COMMIT} 2>/dev/null || git checkout ${LIBWEBRTC_BRANCH}
            gclient sync
            
            # Save to cache after successful fetch
            log_info "Caching WebRTC source for future builds..."
            save_to_cache "${webrtc_dir}" "${WEBRTC_CACHE_FILE}"
        fi
    else
        log_info "WebRTC source already exists, skipping fetch"
    fi

    cd "${webrtc_src}"

    # Step 2: Generate BoringSSL prefix headers (macOS only - to avoid OpenSSL conflicts)
    generate_boringssl_prefix "${webrtc_src}"
    
    # Step 3: Patch BUILD.gn to enable symbol prefixing
    patch_boringssl_buildgn "${webrtc_src}"

    # Step 4: Configure WebRTC build
    log_info "Configuring WebRTC build..."
    
    # Build args from official mediasoup docs for macOS
    # https://mediasoup.org/documentation/v3/libmediasoupclient/installation/
    local build_args="is_debug=false \
symbol_level=0 \
is_component_build=false \
is_clang=true \
rtc_include_tests=false \
rtc_use_h264=true \
use_rtti=true \
use_custom_libcxx=false \
treat_warnings_as_errors=false \
rtc_enable_protobuf=false \
rtc_build_examples=false \
rtc_include_internal_audio_device=true"
    gn gen "${webrtc_out}" --args="${build_args}"
    
    # Step 5: Build WebRTC
    log_info "Building WebRTC (this will take 1-2 hours)..."
    autoninja -C "${webrtc_out}" webrtc

    # Step 6: Build additional targets not included in default webrtc target
    log_info "Building additional required targets..."
    autoninja -C "${webrtc_out}" api:field_trials

    # Step 7: Add field_trials to libwebrtc.a
    log_info "Adding field_trials to libwebrtc.a..."
    ar -r "${webrtc_out}/obj/libwebrtc.a" "${webrtc_out}/obj/api/field_trials/field_trials.o"

    # Step 8: Verify BoringSSL symbols are prefixed
    log_info "Verifying BoringSSL symbol prefixing..."
    local unprefixed_symbols=$(nm -g "${webrtc_out}/obj/libwebrtc.a" 2>/dev/null | grep -E " T _EVP_| T _SSL_| T _CRYPTO_| T _aes_hw| T _sha256_block" | grep -v ${BORINGSSL_PREFIX} || true)
    if [ -n "$unprefixed_symbols" ]; then
        log_warn "Found unprefixed BoringSSL symbols - there may be linking issues:"
        echo "$unprefixed_symbols" | head -10
    else
        log_info "All BoringSSL symbols properly prefixed with ${BORINGSSL_PREFIX}_"
    fi
    
    # Show some prefixed symbols as confirmation
    log_info "Sample prefixed symbols:"
    nm -g "${webrtc_out}/obj/libwebrtc.a" 2>/dev/null | grep "_${BORINGSSL_PREFIX}_EVP_PKEY_free\|_${BORINGSSL_PREFIX}_SSL_new\|_${BORINGSSL_PREFIX}_aes_hw_encrypt" | head -5

    log_info "libwebrtc build complete"
}

# Build libmediasoupclient
build_libmediasoupclient() {
    log_info "Building libmediasoupclient ${LIBMEDIASOUPCLIENT_VERSION}..."
    
    local mediasoup_dir="${BUILD_DIR}/libmediasoupclient"
    local webrtc_src="${BUILD_DIR}/webrtc-checkout/src"
    local webrtc_out="${webrtc_src}/out/${LIBWEBRTC_BRANCH}/obj"
    
    # Check if libwebrtc exists
    if [ ! -d "${webrtc_out}" ]; then
        log_error "libwebrtc not found. Run build-webrtc first."
        return 1
    fi
    
    # Step 1: Clone libmediasoupclient (or restore from cache)
    if [ ! -d "${mediasoup_dir}" ]; then
        if cache_exists "${MEDIASOUP_CACHE_FILE}"; then
            log_info "Found cached libmediasoupclient source, restoring..."
            extract_from_cache "${MEDIASOUP_CACHE_FILE}" "${BUILD_DIR}"
        else
            log_info "Cloning libmediasoupclient..."
            git clone https://github.com/versatica/libmediasoupclient.git "${mediasoup_dir}"
            
            cd "${mediasoup_dir}"
            git fetch --tags
            git checkout "${LIBMEDIASOUPCLIENT_VERSION}"
            
            # Initialize submodules
            log_info "Initializing submodules..."
            git submodule update --init --recursive
            
            # Save to cache
            log_info "Caching libmediasoupclient source..."
            save_to_cache "${mediasoup_dir}" "${MEDIASOUP_CACHE_FILE}"
        fi
    fi
    
    cd "${mediasoup_dir}"
    
    # Ensure correct version
    git checkout "${LIBMEDIASOUPCLIENT_VERSION}" 2>/dev/null || true

    # Apply custom ADM patch (allows passing ADM to internal factory for headless environments)
    local patch_file="${PROJECT_ROOT}/patches/mediasoupclient-adm-option.patch"
    if [ -f "${patch_file}" ]; then
        log_info "Applying mediasoupclient ADM patch..."
        git apply "${patch_file}" || log_warn "Patch already applied or failed"
    fi

    # Step 2: Configure build
    # Disable tests to avoid catch.hpp arm64 asm issue (int $3 -> .inst 0xd4200000)
    # Add BORINGSSL_PREFIX to CXX flags so mediasoupclient uses prefixed symbols
    log_info "Configuring libmediasoupclient build..."
    cmake . -Bbuild \
        -DLIBWEBRTC_INCLUDE_PATH:PATH="${webrtc_src}" \
        -DLIBWEBRTC_BINARY_PATH:PATH="${webrtc_out}" \
        -DCMAKE_CXX_FLAGS="-fvisibility=hidden -DBORINGSSL_PREFIX=${BORINGSSL_PREFIX}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DMEDIASOUPCLIENT_BUILD_TESTS=OFF \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    
    # Step 3: Build
    log_info "Building libmediasoupclient..."
    make -C build -j${NUM_JOBS}
    
    log_info "libmediasoupclient build complete"
}

# Vendor built libraries and headers
vendor_artifacts() {
    log_info "Vendoring build artifacts..."
    
    local webrtc_src="${BUILD_DIR}/webrtc-checkout/src"
    local webrtc_out="${webrtc_src}/out/${LIBWEBRTC_BRANCH}/obj"
    local mediasoup_dir="${BUILD_DIR}/libmediasoupclient"
    
    # Check builds exist
    if [ ! -f "${webrtc_out}/libwebrtc.a" ]; then
        log_error "libwebrtc.a not found"
        return 1
    fi
    
    if [ ! -f "${mediasoup_dir}/build/libmediasoupclient.a" ]; then
        log_error "libmediasoupclient.a not found"
        return 1
    fi

    # Create platform vendor directory
    mkdir -p "${PLATFORM_VENDOR_DIR}/lib"
    mkdir -p "${PLATFORM_VENDOR_DIR}/include"

    # Vendor libraries
    log_info "Copying libraries..."
    cp "${webrtc_out}/libwebrtc.a" "${PLATFORM_VENDOR_DIR}/lib/"
    cp "${mediasoup_dir}/build/libmediasoupclient.a" "${PLATFORM_VENDOR_DIR}/lib/"

    # Vendor libsdptransform (dependency of libmediasoupclient)
    if [ -f "${mediasoup_dir}/build/_deps/libsdptransform-build/libsdptransform.a" ]; then
        cp "${mediasoup_dir}/build/_deps/libsdptransform-build/libsdptransform.a" "${PLATFORM_VENDOR_DIR}/lib/"
        log_info "Copied libsdptransform.a"
    else
        log_error "libsdptransform.a not found - this is required for libmediasoupclient"
        return 1
    fi
    
    # Vendor headers
    log_info "Copying headers..."
    mkdir -p "${PLATFORM_VENDOR_DIR}/include/webrtc"
    
    # Copy only header files, excluding unnecessary directories
    cd "${webrtc_src}"
    find . -type f \( -name "*.h" -o -name "*.hpp" \) | while read -r header; do
        # Skip unnecessary directories
        if [[ "$header" == "./third_party/"* ]] || \
        [[ "$header" == "./build/"* ]] || \
        [[ "$header" == "./buildtools/"* ]] || \
        [[ "$header" == "./out/"* ]] || \
        [[ "$header" == "./tools/"* ]] || \
        [[ "$header" == "./testing/"* ]] || \
        [[ "$header" == "./test/"* ]] || \
        [[ "$header" == *"/."* ]]; then
            continue
        fi
        # Create directory structure
        dir=$(dirname "$header")
        mkdir -p "${PLATFORM_VENDOR_DIR}/include/webrtc/$dir"
        # Copy just the header
        cp "$header" "${PLATFORM_VENDOR_DIR}/include/webrtc/$header"
    done
    
    # Copy BoringSSL prefix header (needed for compilation with BORINGSSL_PREFIX)
    log_info "Copying BoringSSL prefix header..."
    cp "${webrtc_src}/third_party/boringssl/src/include/boringssl_prefix_symbols.h" \
       "${PLATFORM_VENDOR_DIR}/include/webrtc/"
    
    # libmediasoupclient headers
    mkdir -p "${PLATFORM_VENDOR_DIR}/include/mediasoupclient"
    cp -r "${mediasoup_dir}/include/"* "${PLATFORM_VENDOR_DIR}/include/mediasoupclient/"

    # Vendor nlohmann-json v3.11.2 (required by libmediasoupclient)
    log_info "Downloading nlohmann-json v3.11.2..."
    if ! curl -sL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
        -o "${PLATFORM_VENDOR_DIR}/include/json.hpp"; then
        log_error "Failed to download nlohmann-json v3.11.2"
        return 1
    fi
    log_info "nlohmann-json v3.11.2 downloaded successfully"

    # Vendor ONNX Runtime
    vendor_onnxruntime

    # Create vendor manifest
    cat > "${PLATFORM_VENDOR_DIR}/vendor-manifest.txt" << EOF
Vendored on: $(date)
Platform: ${PLATFORM}
libwebrtc: ${LIBWEBRTC_BRANCH} (branch-heads/${LIBWEBRTC_COMMIT})
libmediasoupclient: ${LIBMEDIASOUPCLIENT_VERSION}
nlohmann-json: v3.11.2
onnxruntime: 1.23.2 (CPU)
BoringSSL prefix: ${BORINGSSL_PREFIX}_ (for macOS OpenSSL compatibility)

NOTE: On macOS, all BoringSSL symbols in libwebrtc.a are prefixed with ${BORINGSSL_PREFIX}_
to avoid conflicts with OpenSSL (used by gRPC). When compiling code that includes
WebRTC headers, add -DBORINGSSL_PREFIX=${BORINGSSL_PREFIX} to your compiler flags.
EOF
    
    # Clean up transient build artifacts to save disk space
    log_info "Cleaning up transient build artifacts..."
    rm -rf "${webrtc_src}/out"
    rm -rf "${webrtc_src}/third_party/boringssl/src/build_prefix"
    rm -rf "${mediasoup_dir}/build"
    rm -f /tmp/boringssl_symbols.txt
    log_info "Cleaned up ~10GB of build artifacts"

    log_info "Vendoring complete!"
    log_info "Libraries vendored to: ${PLATFORM_VENDOR_DIR}/lib/"
    log_info "Headers vendored to: ${PLATFORM_VENDOR_DIR}/include/"
    log_warn ""
    log_warn "IMPORTANT: Add BORINGSSL_PREFIX=${BORINGSSL_PREFIX} to your CMake compile definitions:"
    log_warn "  target_compile_definitions(your_target PRIVATE BORINGSSL_PREFIX=${BORINGSSL_PREFIX})"
}

# Download and vendor ONNX Runtime
vendor_onnxruntime() {
    log_info "Downloading ONNX Runtime 1.23.2..."
    
    local onnx_version="1.23.2"
    local onnx_tarball="onnxruntime-osx-arm64-${onnx_version}.tgz"
    local onnx_url="https://github.com/microsoft/onnxruntime/releases/download/v${onnx_version}/${onnx_tarball}"
    local onnx_dir="${BUILD_DIR}/onnxruntime-osx-arm64-${onnx_version}"
    
    cd "${BUILD_DIR}"
    
    # Check cache first
    if cache_exists "${onnx_tarball}"; then
        log_info "Using cached ONNX Runtime..."
        cp "${VENDOR_CACHE_DIR}/${onnx_tarball}" .
    elif [ ! -f "${onnx_tarball}" ]; then
        curl -LO "${onnx_url}"
        # Cache for future use
        cp "${onnx_tarball}" "${VENDOR_CACHE_DIR}/"
    fi
    
    # Extract
    tar xzf "${onnx_tarball}"
    
    # Copy library
    cp "${onnx_dir}/lib/libonnxruntime.${onnx_version}.dylib" "${PLATFORM_VENDOR_DIR}/lib/"
    cd "${PLATFORM_VENDOR_DIR}/lib/"
    ln -sf "libonnxruntime.${onnx_version}.dylib" libonnxruntime.dylib
    ln -sf "libonnxruntime.${onnx_version}.dylib" libonnxruntime.1.dylib
    cd -
    
    # Copy headers
    mkdir -p "${PLATFORM_VENDOR_DIR}/include/onnxruntime"
    cp -r "${onnx_dir}/include/"* "${PLATFORM_VENDOR_DIR}/include/onnxruntime/"
    
    # Cleanup extracted dir (keep tarball in cache)
    rm -rf "${onnx_dir}" "${onnx_tarball}"
    
    log_info "ONNX Runtime ${onnx_version} vendored successfully"
}

# Show cache status
show_cache_status() {
    log_info "Vendor cache directory: ${VENDOR_CACHE_DIR}"
    echo
    
    if [ ! -d "${VENDOR_CACHE_DIR}" ]; then
        log_warn "Cache directory does not exist yet"
        return
    fi
    
    echo "Cached files:"
    if cache_exists "${WEBRTC_CACHE_FILE}"; then
        local size=$(du -h "${VENDOR_CACHE_DIR}/${WEBRTC_CACHE_FILE}" | cut -f1)
        echo "  ✓ ${WEBRTC_CACHE_FILE} (${size})"
    else
        echo "  ✗ ${WEBRTC_CACHE_FILE} (not cached)"
    fi
    
    if cache_exists "${MEDIASOUP_CACHE_FILE}"; then
        local size=$(du -h "${VENDOR_CACHE_DIR}/${MEDIASOUP_CACHE_FILE}" | cut -f1)
        echo "  ✓ ${MEDIASOUP_CACHE_FILE} (${size})"
    else
        echo "  ✗ ${MEDIASOUP_CACHE_FILE} (not cached)"
    fi
    
    local onnx_tarball="onnxruntime-osx-arm64-1.23.2.tgz"
    if cache_exists "${onnx_tarball}"; then
        local size=$(du -h "${VENDOR_CACHE_DIR}/${onnx_tarball}" | cut -f1)
        echo "  ✓ ${onnx_tarball} (${size})"
    else
        echo "  ✗ ${onnx_tarball} (not cached)"
    fi
    
    echo
    local total_size=$(du -sh "${VENDOR_CACHE_DIR}" 2>/dev/null | cut -f1 || echo "0")
    echo "Total cache size: ${total_size}"
}

# Clear cache
clear_cache() {
    if [ -d "${VENDOR_CACHE_DIR}" ]; then
        local size=$(du -sh "${VENDOR_CACHE_DIR}" | cut -f1)
        log_warn "This will delete ${size} of cached files"
        read -p "Clear vendor cache? (y/N) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf "${VENDOR_CACHE_DIR}"
            log_info "Cache cleared"
        fi
    else
        log_info "Cache directory does not exist"
    fi
}

# Main execution
main() {
    local command="${1:-}"

    # Verify we're on macOS arm64
    if [[ "$(uname -s)" != "Darwin" ]] || [[ "$(uname -m)" != "arm64" ]]; then
        log_error "This script is for macOS arm64 only"
        log_error "For Linux, use vendor-webrtc-deps-linux.sh instead"
        exit 1
    fi
    
    case "${command}" in
        setup)
            log_info "Starting dependency build setup for media engine"
            log_info "Detected platform: ${PLATFORM}"
            
            check_prerequisites
            setup_build_env
            clean_build
            
            # Create platform-specific vendor directory
            mkdir -p "${PLATFORM_VENDOR_DIR}/include"
            mkdir -p "${PLATFORM_VENDOR_DIR}/lib"
            
            log_info "Build environment ready"
            log_info "Build directory: ${BUILD_DIR}"
            log_info "Vendor directory: ${PLATFORM_VENDOR_DIR}"
            log_info "Cache directory: ${VENDOR_CACHE_DIR}"
            log_info "Using ${NUM_JOBS} parallel jobs"
            
            echo
            log_warn "Next steps:"
            log_warn "  1. $0 build-webrtc  # Build libwebrtc (1-2 hours)"
            log_warn "  2. $0 build-mediasoup  # Build libmediasoupclient"
            log_warn "  3. $0 vendor  # Copy artifacts to vendor directory"
            ;;
            
        build-webrtc)
            setup_build_env
            build_libwebrtc
            ;;
            
        build-mediasoup)
            setup_build_env
            build_libmediasoupclient
            ;;
            
        vendor)
            vendor_artifacts
            ;;
            
        cache-status)
            show_cache_status
            ;;
            
        cache-clear)
            clear_cache
            ;;
            
        all)
            log_info "Running complete build process"
            echo
            log_warn "This build will:"
            log_warn "  - Download ~30GB of source code (unless cached)"
            log_warn "  - Take 2-4 hours to complete"
            log_warn "  - Require ~50GB of disk space"
            log_warn "  - Prefix BoringSSL symbols with ${BORINGSSL_PREFIX}_ (for OpenSSL compatibility)"
            echo
            show_cache_status
            echo
            read -p "Continue with build? (y/N) " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                log_info "Build cancelled"
                exit 0
            fi
            
            check_prerequisites
            setup_build_env
            clean_build
            
            # Create platform-specific vendor directory
            mkdir -p "${PLATFORM_VENDOR_DIR}/include"
            mkdir -p "${PLATFORM_VENDOR_DIR}/lib"
            
            # Build in order
            build_libwebrtc
            if [ $? -eq 0 ]; then
                build_libmediasoupclient
                if [ $? -eq 0 ]; then
                    vendor_artifacts
                fi
            fi
            
            log_info "Build process complete!"
            ;;

        docker)
            # Non-interactive build for Docker images
            # Skips prompts and prerequisite checks (Docker handles deps)
            log_info "Running Docker build (non-interactive)..."
            setup_build_env
            build_libwebrtc
            build_libmediasoupclient
            vendor_artifacts
            log_info "Docker build complete!"
            ;;

        *)
            log_error "Usage: $0 {setup|build-webrtc|build-mediasoup|vendor|cache-status|cache-clear|all|docker}"
            echo
            echo "Commands:"
            echo "  setup          - Initialize build environment"
            echo "  build-webrtc   - Build libwebrtc with BoringSSL symbol prefixing (1-2 hours, uses cache if available)"
            echo "  build-mediasoup - Build libmediasoupclient"
            echo "  vendor         - Copy built artifacts to vendor directory"
            echo "  cache-status   - Show what's in the vendor cache"
            echo "  cache-clear    - Clear the vendor cache"
            echo "  all            - Run complete build process (interactive)"
            echo "  docker         - Run complete build process (non-interactive, for Docker)"
            echo
            echo "Cache location: ${VENDOR_CACHE_DIR}"
            echo
            echo "Note: On macOS, BoringSSL symbols are prefixed with ${BORINGSSL_PREFIX}_ to avoid"
            echo "      conflicts with OpenSSL (used by gRPC via vcpkg)."
            exit 1
            ;;
    esac
}

main "$@"
