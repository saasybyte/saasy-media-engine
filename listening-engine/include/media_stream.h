#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <boost/circular_buffer.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <mutex>
#include <thread>

#include "protos/listening_engine/v1/listening_engine.grpc.pb.h"  // IWYU pragma: keep

namespace saasy::listening_engine {

// Forward declarations
struct Session;

struct MediaStreamStats {
  std::atomic<uint64_t> frames_sent{0};
  std::atomic<uint64_t> frames_dropped{0};
  std::atomic<uint64_t> bytes_sent{0};
  std::chrono::steady_clock::time_point start_time;

  void Reset() {
    frames_sent = 0;
    frames_dropped = 0;
    bytes_sent = 0;
    start_time = std::chrono::steady_clock::now();
  }

  double GetFrameRate() const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();
    return elapsed > 0 ? static_cast<double>(frames_sent) / elapsed : 0.0;
  }
};

class MediaStream {
 public:
  MediaStream(const std::string& session_id, Session* session,
              grpc::ServerReaderWriter<v1::ListeningEngineMediaPayload, v1::ListeningEngineMediaAck>* stream);

  ~MediaStream();

  void Start();

  void Stop();

  const MediaStreamStats& GetStats() const { return stats_; }

  void OnVadTurnEvent(const std::string& event_type, uint64_t timestamp_ms);

 private:
  static constexpr size_t kLogInterval = 1000;
  static constexpr size_t kMaxFrameSize = 1024 * 1024; // 1MB max frame size
  static constexpr size_t kPreRollMs = 500;
  static constexpr size_t kPreRollSamples = 48000 * kPreRollMs / 1000; // 24,000 samples
  static constexpr size_t kPreRollBytes = kPreRollSamples * 2; // 16-bit = 2 bytes/sample

  std::string session_id_;
  Session* session_;
  grpc::ServerReaderWriter<v1::ListeningEngineMediaPayload, v1::ListeningEngineMediaAck>* stream_;
  std::atomic<bool> running_{false};
  std::thread media_thread_;
  std::mutex write_mutex_;
  MediaStreamStats stats_;
  std::atomic<uint64_t> request_counter_{0};
  boost::circular_buffer<uint8_t> pre_roll_buffer_{kPreRollBytes};
  std::atomic<bool> gate_open_{false};
  std::mutex pre_roll_mutex_; 

  void ForwardMediaStream();

  void SendMediaFrame(const std::string& device_consumer_id, const std::vector<uint8_t>& frame_data,
                      uint32_t sample_rate, uint32_t channels, uint64_t timestamp_ms);
    
  std::string GenerateRequestId();

  void FlushPreRollBuffer();
};

}  // namespace saasy::listening_engine
