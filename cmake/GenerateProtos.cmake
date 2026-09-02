# Proto source files
set(SPEAKING_ENGINE_PROTO ${PROTO_DIR}/protos/speaking_engine/v1/speaking_engine.proto)
set(LISTENING_ENGINE_PROTO ${PROTO_DIR}/protos/listening_engine/v1/listening_engine.proto)
set(SHARED_PROTO ${PROTO_DIR}/protos/shared/v1/shared.proto)

# Output file paths for Speaking Engine
set(SPEAKING_ENGINE_PROTO_SRCS "${PROTO_GENERATED_DIR}/protos/speaking_engine/v1/speaking_engine.pb.cc")
set(SPEAKING_ENGINE_PROTO_HDRS "${PROTO_GENERATED_DIR}/protos/speaking_engine/v1/speaking_engine.pb.h")
set(SPEAKING_ENGINE_GRPC_SRCS "${PROTO_GENERATED_DIR}/protos/speaking_engine/v1/speaking_engine.grpc.pb.cc")
set(SPEAKING_ENGINE_GRPC_HDRS "${PROTO_GENERATED_DIR}/protos/speaking_engine/v1/speaking_engine.grpc.pb.h")

# Output file paths for Listening Engine
set(LISTENING_ENGINE_PROTO_SRCS "${PROTO_GENERATED_DIR}/protos/listening_engine/v1/listening_engine.pb.cc")
set(LISTENING_ENGINE_PROTO_HDRS "${PROTO_GENERATED_DIR}/protos/listening_engine/v1/listening_engine.pb.h")
set(LISTENING_ENGINE_GRPC_SRCS "${PROTO_GENERATED_DIR}/protos/listening_engine/v1/listening_engine.grpc.pb.cc")
set(LISTENING_ENGINE_GRPC_HDRS "${PROTO_GENERATED_DIR}/protos/listening_engine/v1/listening_engine.grpc.pb.h")

# Output file paths for Shared
set(SHARED_PROTO_SRCS "${PROTO_GENERATED_DIR}/protos/shared/v1/shared.pb.cc")
set(SHARED_PROTO_HDRS "${PROTO_GENERATED_DIR}/protos/shared/v1/shared.pb.h")

# All proto sources for the library
set(PROTO_SRCS
    ${SPEAKING_ENGINE_PROTO_SRCS}
    ${SPEAKING_ENGINE_GRPC_SRCS}
    ${LISTENING_ENGINE_PROTO_SRCS}
    ${LISTENING_ENGINE_GRPC_SRCS}
    ${SHARED_PROTO_SRCS}
)

# Custom command to generate proto files
add_custom_command(
    OUTPUT ${SPEAKING_ENGINE_PROTO_SRCS} ${SPEAKING_ENGINE_PROTO_HDRS}
           ${SPEAKING_ENGINE_GRPC_SRCS} ${SPEAKING_ENGINE_GRPC_HDRS}
           ${LISTENING_ENGINE_PROTO_SRCS} ${LISTENING_ENGINE_PROTO_HDRS}
           ${LISTENING_ENGINE_GRPC_SRCS} ${LISTENING_ENGINE_GRPC_HDRS}
           ${SHARED_PROTO_SRCS} ${SHARED_PROTO_HDRS}
    COMMAND protobuf::protoc
    ARGS --grpc_out=${PROTO_GENERATED_DIR}
         --cpp_out=${PROTO_GENERATED_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         -I${PROTO_DIR}
         ${SPEAKING_ENGINE_PROTO}
         ${LISTENING_ENGINE_PROTO}
         ${SHARED_PROTO}
    DEPENDS ${SPEAKING_ENGINE_PROTO} ${LISTENING_ENGINE_PROTO} ${SHARED_PROTO}
    COMMENT "Generating protobuf and gRPC sources"
)

# Create proto library
add_library(saasy_proto STATIC ${PROTO_SRCS})
target_link_libraries(saasy_proto PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)
target_include_directories(saasy_proto PUBLIC ${PROTO_GENERATED_DIR})
