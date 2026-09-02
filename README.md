# saasy-media-engine

Intelligence-plane C++ media engines for [SaasyByte](https://github.com/saasybyte/saasybyte), an open-source real-time AI voice platform.

Two long-running services built on libwebrtc and libmediasoupclient, connecting to the SFU as first-class WebRTC peers:

- **Listening Engine** (the AI's ears): consumes the user's RTP audio directly from the SFU, runs Silero VAD and SmartTurn turn detection locally via ONNX Runtime, and gates audio to the Orchestrator so only speech between turn boundaries is forwarded for transcription.
- **Speaking Engine** (the AI's mouth): receives synthesized audio from the Orchestrator and plays it into the session through a custom clock-driven audio device module pulling from a lock-free queue.

Separate processes by design: a crash in playback never kills inbound processing, and the CPU-bound listening path never competes with real-time playout scheduling.

## How It Fits

- **Controlled by saasy-orchestrator** (gRPC over Unix Domain Sockets): receives commands, emits VAD events and audio.
- **Connects to saasy-sfu** as a mediasoup WebRTC client (RTP media plane).
- **Proto schema** from the [saasy-proto](https://github.com/saasybyte/saasy-proto) git submodule.

See the [platform overview](https://github.com/saasybyte/saasybyte) for the full architecture.

## Build

Platforms: Linux x64 and macOS arm64.

Requirements: CMake 3.20+, Ninja, clang, vcpkg dependencies (gRPC, protobuf, boost, spdlog; resolved via the manifest).

The engines link against prebuilt vendored libraries (libwebrtc, libmediasoupclient, libsdptransform, ONNX Runtime). Two ways to get them:

```bash
make fetch-prebuilt   # fast path: downloads the platform tarball from GitHub Releases
make vendor-deps      # slow path: builds libwebrtc + libmediasoupclient from source (2-4 hours, ~50 GB disk)
```

Then:

```bash
git submodule update --init   # saasy-proto
make configure                # cmake with vcpkg (auto-detects platform)
make build                    # build both engines
make run-listening            # or run-speaking
```

Docker images build via `docker/listening-engine/Dockerfile` and `docker/speaking-engine/Dockerfile`, using the prebuilt vendor and builder-deps images from GHCR.

## Third-Party

This repository builds against and redistributes artifacts from: libwebrtc (BSD-3-Clause), libmediasoupclient (ISC, with a small local patch in `patches/`), libsdptransform, ONNX Runtime (MIT), nlohmann-json (MIT), and libsamplerate (BSD-2-Clause), plus vcpkg-managed dependencies under their respective licenses. The ONNX models in `listening-engine/models/` are Silero VAD (MIT) and SmartTurn v3 (BSD-2-Clause).

## License

Apache-2.0, see [LICENSE](LICENSE). Third-party components remain under their own licenses listed above.
