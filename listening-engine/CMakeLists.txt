# Listening Engine executable
add_executable(listening-engine
    src/main.cpp
    src/listening_engine_service.cpp
    src/commands.cpp
    src/audio_track_sink.cpp
    src/custom_audio_device_module.cpp
    src/session.cpp
    src/streams.cpp
    src/control_stream.cpp
    src/event_stream.cpp
    src/media_stream.cpp
    src/vad_turn/log_mel_spectrogram.cpp
    src/vad_turn/silero_vad.cpp
    src/vad_turn/smart_turn_detector.cpp
    src/vad_turn/audio_resampler.cpp
    src/vad_turn/audio_turn_buffer.cpp
    src/vad_turn/vad_turn_pipeline.cpp
)

# WebRTC required compile definitions
if(APPLE)
    target_compile_definitions(listening-engine PRIVATE
        WEBRTC_POSIX
        WEBRTC_MAC
        NDEBUG
        BORINGSSL_PREFIX=BSSL
    )
else()
    target_compile_definitions(listening-engine PRIVATE
        WEBRTC_POSIX
        WEBRTC_LINUX
        NDEBUG
    )
endif()

# Link options to handle symbol conflicts (Linux only)
if(NOT APPLE)
    target_link_options(listening-engine PRIVATE
        -Wl,--allow-multiple-definition
    )
endif()

# Link against all required libraries
target_link_libraries(listening-engine PRIVATE
    saasy_common
    saasy_proto
    spdlog::spdlog
    mediasoupclient
    sdptransform
    webrtc
    gRPC::grpc++
    samplerate
    onnxruntime
    Boost::system
    Boost::thread
    Boost::lockfree
)

# Platform-specific libraries
if(APPLE)
    target_link_libraries(listening-engine PRIVATE
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
    target_link_libraries(listening-engine PRIVATE
        pthread
        dl
        X11
    )
endif()

# Include directories
target_include_directories(listening-engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PREBUILT_DIR}/include
    ${PREBUILT_DIR}/include/webrtc
    ${PREBUILT_DIR}/include/mediasoupclient
    ${PREBUILT_DIR}/include/onnxruntime
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(listening-engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation rules
install(TARGETS listening-engine RUNTIME DESTINATION bin)
