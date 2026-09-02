#!/bin/bash
set -euo pipefail

# =======================================================================================
# This script builds libwebrtc and libmediasoupclient from source and
# vendors the required static libraries and headers into the repository
# for consistent builds.
#
# Following: https://mediasoup.org/documentation/v3/libmediasoupclient/installation/
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
readonly NUM_JOBS=$(nproc)

# Versions
readonly LIBWEBRTC_BRANCH="m140"
readonly LIBWEBRTC_COMMIT="7339"
readonly LIBMEDIASOUPCLIENT_VERSION="3.5.0"

# Cache file names (versioned)
readonly WEBRTC_CACHE_FILE="webrtc-checkout-${LIBWEBRTC_BRANCH}-${LIBWEBRTC_COMMIT}-linux.tar.gz"
readonly MEDIASOUP_CACHE_FILE="libmediasoupclient-${LIBMEDIASOUPCLIENT_VERSION}.tar.gz"

# Platform
readonly PLATFORM="linux-x64"
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
    command -v clang >/dev/null 2>&1 || missing_deps+=("clang")
    command -v ninja >/dev/null 2>&1 || missing_deps+=("ninja-build")
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Install with: sudo apt-get install ${missing_deps[*]}"
        exit 1
    fi
    
    # Check disk space (need ~50GB for WebRTC build)
    local available_gb=$(df -BG "${PROJECT_ROOT}" | awk 'NR==2 {print $4}' | sed 's/G//')
    if [ "$available_gb" -lt 50 ]; then
        log_warn "Low disk space: ${available_gb}GB available (50GB recommended)"
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# Create build environment
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
    
    # Step 2: Disable CREL relocations (GNU ld and runtime linker don't support them)
    cd "${webrtc_src}"
    log_info "Disabling CREL relocations..."
    sed -i '/-Wa,--crel,--allow-experimental-crel/d' build/config/compiler/BUILD.gn

    # Step 3: Configure build
    cd "${webrtc_src}"
    log_info "Configuring WebRTC build..."
    
    # Build args optimized for libmediasoupclient
    local build_args="is_debug=false \
symbol_level=0 \
is_component_build=false \
is_clang=true \
use_clang_modules=false \
rtc_include_tests=false \
rtc_use_h264=true \
use_rtti=true \
use_custom_libcxx=false \
treat_warnings_as_errors=false \
use_ozone=true \
rtc_enable_protobuf=false \
rtc_build_examples=false \
rtc_use_pulseaudio=true \
rtc_use_alsa=true \
rtc_use_pipewire=false \
rtc_include_pulse_audio=true \
rtc_include_internal_audio_device=true \
rtc_use_x11=true"
    
    gn gen "${webrtc_out}" --args="${build_args}"
    
    # Step 4: Build
    log_info "Building WebRTC (this will take 1-2 hours)..."
    autoninja -C "${webrtc_out}"

    # Step 5: Build additional targets not included in default webrtc target
    log_info "Building additional required targets..."
    autoninja -C "${webrtc_out}" api:field_trials

    # Step 6: Add field_trials to libwebrtc.a
    log_info "Adding field_trials to libwebrtc.a..."
    ar -r "${webrtc_out}/obj/libwebrtc.a" "${webrtc_out}/obj/api/field_trials/field_trials.o"
    
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
        log_error "libwebrtc not found. Run build_libwebrtc first."
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
    log_info "Configuring libmediasoupclient build..."
    cmake . -Bbuild \
        -DLIBWEBRTC_INCLUDE_PATH:PATH="${webrtc_src}" \
        -DLIBWEBRTC_BINARY_PATH:PATH="${webrtc_out}" \
        -DCMAKE_CXX_FLAGS="-fvisibility=hidden -fpermissive" \
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
    
    # libmediasoupclient headers
    mkdir -p "${PLATFORM_VENDOR_DIR}/include/mediasoupclient"
    cp -r "${mediasoup_dir}/include/"* "${PLATFORM_VENDOR_DIR}/include/mediasoupclient/"

    # Vendor nlohmann-json v3.11.2 (required by libmediasoupclient)
    log_info "Downloading nlohmann-json v3.11.2..."
    if ! wget -q https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
        -O "${PLATFORM_VENDOR_DIR}/include/json.hpp"; then
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
EOF

    # Clean up transient build artifacts to save disk space
    log_info "Cleaning up transient build artifacts..."
    rm -rf "${webrtc_src}/out"
    rm -rf "${mediasoup_dir}/build"
    log_info "Cleaned up ~10GB of build artifacts"

    log_info "Vendoring complete!"
    log_info "Libraries vendored to: ${PLATFORM_VENDOR_DIR}/lib/"
    log_info "Headers vendored to: ${PLATFORM_VENDOR_DIR}/include/"
}

# Download and vendor ONNX Runtime
vendor_onnxruntime() {
    log_info "Downloading ONNX Runtime 1.23.2..."
    
    local onnx_version="1.23.2"
    local onnx_tarball="onnxruntime-linux-x64-${onnx_version}.tgz"
    local onnx_url="https://github.com/microsoft/onnxruntime/releases/download/v${onnx_version}/${onnx_tarball}"
    local onnx_dir="${BUILD_DIR}/onnxruntime-linux-x64-${onnx_version}"
    
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
    
    # Copy libraries
    cp "${onnx_dir}/lib/libonnxruntime.so.${onnx_version}" "${PLATFORM_VENDOR_DIR}/lib/"
    cp "${onnx_dir}/lib/libonnxruntime_providers_shared.so" "${PLATFORM_VENDOR_DIR}/lib/"
    cd "${PLATFORM_VENDOR_DIR}/lib/"
    ln -sf "libonnxruntime.so.${onnx_version}" libonnxruntime.so
    ln -sf "libonnxruntime.so.${onnx_version}" libonnxruntime.so.1
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
    
    local onnx_tarball="onnxruntime-linux-x64-1.23.2.tgz"
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

    # Verify we're on Linux x64
    if [[ "$(uname -s)" != "Linux" ]] || [[ "$(uname -m)" != "x86_64" ]]; then
        log_error "This script is for Linux x64 only"
        log_error "For macOS, use vendor-webrtc-deps-macos.sh instead"
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
            echo "  build-webrtc   - Build libwebrtc (1-2 hours, uses cache if available)"
            echo "  build-mediasoup - Build libmediasoupclient"
            echo "  vendor         - Copy built artifacts to vendor directory"
            echo "  cache-status   - Show what's in the vendor cache"
            echo "  cache-clear    - Clear the vendor cache"
            echo "  all            - Run complete build process (interactive)"
            echo "  docker         - Run complete build process (non-interactive, for Docker)"
            echo
            echo "Cache location: ${VENDOR_CACHE_DIR}"
            exit 1
            ;;
    esac
}

main "$@"
