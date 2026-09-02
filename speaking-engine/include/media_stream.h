#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "protos/speaking_engine/v1/speaking_engine.grpc.pb.h"  // IWYU pragma: keep

namespace saasy::speaking_engine {

// Forward declarations
struct Session;

struct MediaStreamStats {
  std::atomic<uint64_t> frames_received{0};
  std::atomic<uint64_t> frames_dropped{0};
  std::atomic<uint64_t> bytes_received{0};
  std::chrono::steady_clock::time_point start_time;

  void Reset() {
    frames_received = 0;
    frames_dropped = 0;
    bytes_received = 0;
    start_time = std::chrono::steady_clock::now();
  }

  double GetFrameRate() const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();
    return elapsed > 0 ? static_cast<double>(frames_received) / elapsed : 0.0;
  }
};

class MediaStream {
 public:
  MediaStream(
      const std::string& session_id, Session* session,
      grpc::ServerReaderWriter<v1::SpeakingEngineMediaAck, v1::SpeakingEngineMediaPayload>* stream);

  ~MediaStream();

  void Start();

  void Stop();

  const MediaStreamStats& GetStats() const { return stats_; }

 private:
  void ProcessMediaStream();

  void HandleMediaFrame(const v1::MediaFrame& frame, const std::string& request_id);

  void SendErrorResponse(const std::string& request_id, const std::string& participant_id,
                         const std::string& error_code, const std::string& error_message);

  bool ValidateMediaFrame(const v1::MediaFrame& frame);

  std::string session_id_;
  Session* session_;
  grpc::ServerReaderWriter<v1::SpeakingEngineMediaAck, v1::SpeakingEngineMediaPayload>* stream_;
  std::atomic<bool> running_{false};
  std::thread media_thread_;
  std::mutex write_mutex_;
  MediaStreamStats stats_;
  static constexpr size_t kLogInterval = 1000;
  static constexpr size_t kMaxFrameSize = 1024 * 1024; // 1MB max frame size
};

}  // namespace saasy::speaking_engine
