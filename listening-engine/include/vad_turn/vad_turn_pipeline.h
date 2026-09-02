#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vad_turn/audio_resampler.h"
#include "vad_turn/audio_turn_buffer.h"
#include "vad_turn/silero_vad.h"
#include "vad_turn/smart_turn_detector.h"

namespace saasy::listening_engine {

// Forward declaration
struct Session;

class VadTurnPipeline {
 public:
  VadTurnPipeline(const std::string& session_id,
                  std::shared_ptr<SileroVAD> shared_vad,
                  std::shared_ptr<SmartTurnDetector> shared_turn_detector,
                  Session* session);

  ~VadTurnPipeline();

  VadTurnPipeline(const VadTurnPipeline&) = delete;

  VadTurnPipeline& operator=(const VadTurnPipeline&) = delete;

  VadTurnPipeline(VadTurnPipeline&&) = delete;

  VadTurnPipeline& operator=(VadTurnPipeline&&) = delete;

  void Start();

  void Stop();

  void ProcessAudio(const int16_t* audio, size_t samples);

 private:
  static constexpr size_t kVadChunkSize = 512; // VAD chunk size (32ms @ 16kHz)
  static constexpr uint32_t kMinUtteranceDurationMs = 400; // Backchannel filtering: ignore utterances shorter than this
  static constexpr uint32_t kSilenceTimeoutBaseMs = 1000; // Fallback timeout for when SmartTurn isn't confident
  static constexpr uint32_t kSilenceTimeoutExtendedMs = 3000; // Extended fallback timeout for mid-sentence pauses

  std::string session_id_;
  Session* session_;
  std::shared_ptr<SileroVAD> shared_vad_;
  std::shared_ptr<SmartTurnDetector> shared_turn_detector_;
  std::unique_ptr<AudioResampler> resampler_;
  std::unique_ptr<AudioTurnBuffer> turn_buffer_;
  std::vector<float> vad_accumulator_;
  std::mutex accumulator_mutex_;
  VadState current_state_;
  std::chrono::steady_clock::time_point speech_start_time_;
  std::chrono::steady_clock::time_point silence_start_time_;
  std::atomic<bool> running_{false};

  void ProcessVadChunk(const std::vector<float>& chunk);

  void HandleSpeechDetected(float probability);

  void HandleSilenceDetected();

  void TriggerSmartTurnDetection();

  uint64_t GetCurrentTimestampMs();
};

}  // namespace saasy::listening_engine
