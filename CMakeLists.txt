cmake_minimum_required(VERSION 3.20)
project(saasy_media_engine VERSION 0.1.0 LANGUAGES CXX)

# Set C++ compiler options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required third-party packages via vcpkg or system
find_package(gRPC CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread lockfree)

# Platform detection for prebuilt libraries
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/prebuilt/darwin-arm64)
    set(ONNX_LIB_NAME libonnxruntime.dylib)
elseif(UNIX AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/prebuilt/linux-x64)
    set(ONNX_LIB_NAME libonnxruntime.so)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Using prebuilt libraries from: ${PREBUILT_DIR}")

# Create imported targets for vendored libraries (add GLOBAL for visibility in subdirs)
add_library(webrtc STATIC IMPORTED GLOBAL)
set_target_properties(webrtc PROPERTIES
    IMPORTED_LOCATION ${PREBUILT_DIR}/lib/libwebrtc.a
)

add_library(mediasoupclient STATIC IMPORTED GLOBAL)
set_target_properties(mediasoupclient PROPERTIES
    IMPORTED_LOCATION ${PREBUILT_DIR}/lib/libmediasoupclient.a
)

# Add libsdptransform (required by mediasoupclient)
add_library(sdptransform STATIC IMPORTED GLOBAL)
set_target_properties(sdptransform PROPERTIES
    IMPORTED_LOCATION ${PREBUILT_DIR}/lib/libsdptransform.a
)

# Add ONNX Runtime (shared library)
add_library(onnxruntime SHARED IMPORTED GLOBAL)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION ${PREBUILT_DIR}/lib/${ONNX_LIB_NAME}
)

# Fetch libsamplerate from source
include(FetchContent)
FetchContent_Declare(
    libsamplerate
    GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
    GIT_TAG 0.2.2
)
FetchContent_MakeAvailable(libsamplerate)

# Proto configuration - these variables are used by GenerateProtos.cmake
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/saasy-proto)
set(PROTO_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GENERATED_DIR})

# Include proto generation
include(cmake/GenerateProtos.cmake)

# Add common subdirectory
add_subdirectory(common)

# Add subdirectories for services
add_subdirectory(speaking-engine)
add_subdirectory(listening-engine)
