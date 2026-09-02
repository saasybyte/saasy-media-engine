# Common library for shared components
add_library(saasy_common STATIC
    src/command_framework.cpp
    src/json_proto_converter.cpp
    src/proto_json_converter.cpp
    src/webrtc_factory.cpp
)

# WebRTC required compile definitions
if(APPLE)
    target_compile_definitions(saasy_common PRIVATE
        WEBRTC_POSIX
        WEBRTC_MAC
        NDEBUG
        BORINGSSL_PREFIX=BSSL
    )
else()
    target_compile_definitions(saasy_common PRIVATE
        WEBRTC_POSIX
        WEBRTC_LINUX
        NDEBUG
    )
endif()

# Link against required libraries
target_link_libraries(saasy_common PUBLIC
    saasy_proto
    mediasoupclient
    webrtc
)

# Include directories
target_include_directories(saasy_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PREBUILT_DIR}/include
    ${PREBUILT_DIR}/include/webrtc
    ${PREBUILT_DIR}/include/mediasoupclient
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(saasy_common PRIVATE -Wall -Wextra -Wpedantic)
endif()
