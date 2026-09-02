# Speaking Engine executable
add_executable(speaking-engine
    src/main.cpp
    src/speaking_engine_service.cpp
    src/commands.cpp
    src/custom_audio_device_module.cpp
    src/session.cpp
    src/streams.cpp
    src/event_stream.cpp
    src/media_stream.cpp
    src/control_stream.cpp
)

# WebRTC required compile definitions
if(APPLE)
    target_compile_definitions(speaking-engine PRIVATE
        WEBRTC_POSIX
        WEBRTC_MAC
        NDEBUG
        BORINGSSL_PREFIX=BSSL
    )
else()
    target_compile_definitions(speaking-engine PRIVATE
        WEBRTC_POSIX
        WEBRTC_LINUX
        NDEBUG
    )
endif()

# Link options to handle symbol conflicts (Linux only)
if(NOT APPLE)
    target_link_options(speaking-engine PRIVATE
        -Wl,--allow-multiple-definition
    )
endif()

# Link against all required libraries
target_link_libraries(speaking-engine PRIVATE
    saasy_common
    saasy_proto
    spdlog::spdlog
    mediasoupclient
    sdptransform
    webrtc
    gRPC::grpc++
    Boost::system
    Boost::thread
    Boost::lockfree
)

# Platform-specific libraries
if(APPLE)
    target_link_libraries(speaking-engine PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework Foundation"
        "-framework AVFoundation"
    )
else()
    target_link_libraries(speaking-engine PRIVATE
        pthread
        dl
        X11
    )
endif()

# Include directories
target_include_directories(speaking-engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PREBUILT_DIR}/include
    ${PREBUILT_DIR}/include/webrtc
    ${PREBUILT_DIR}/include/mediasoupclient
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(speaking-engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation rules
install(TARGETS speaking-engine RUNTIME DESTINATION bin)
