# saasy-media-engine

## Commands
- `make configure` — detect platform, run cmake with vcpkg
- `make reconfigure` — clean and reconfigure
- `make build` — build both engines
- `make build-speaking` / `make build-listening` — build individual engine
- `make run-speaking` / `make run-listening` — run individual engine
- `make vendor-deps` — build and vendor WebRTC deps (auto-detects platform)
- `make clean` — remove build artifacts

## Conventions
- **Namespaces**: `saasy::speaking_engine`, `saasy::listening_engine`, `saasy::common`.
- **Formatting**: `.clang-format` in repo root (Google style, 2-space indent, 100 col limit).
- **Logging**: `std::cout`/`std::cerr` with `[ClassName]` prefix. spdlog is a CMake dependency but not used in application code.
- **Command pattern**: commands in `commands.h`, extend `common::CommandWithResponse<T>`, processed via `common/include/command_framework.h`.
- **Stream pattern**: each engine has ControlStream, EventStream, MediaStream coordinated by a `Streams` class per session.
- **Proto types**: generated from `third_party/saasy-proto/` submodule. Include as `protos/<engine>/v1/<engine>.pb.h`. Do not define proto types locally.
- **Platforms**: macOS arm64 and Linux x64 only.
- **BoringSSL/OpenSSL conflict (macOS)**: libwebrtc bundles BoringSSL; gRPC links OpenSSL via vcpkg. All BoringSSL symbols are prefixed with `BSSL_` during vendor build. Code including WebRTC headers must compile with `-DBORINGSSL_PREFIX=BSSL`.
- **libsamplerate**: fetched via CMake `FetchContent`, not vendored.

## Service Boundaries
- **Controlled by saasy-orchestrator** (gRPC over UDS): receives commands, sends audio/events.
- **Connects to saasy-sfu** as mediasoup WebRTC client (RTP media plane).
- **Proto schema from saasy-proto** (git submodule): do not define proto types locally.
- **Does not own**: signaling (saasy-signal), media forwarding (saasy-sfu), AI inference (saasy-orchestrator), proto schema (saasy-proto).
