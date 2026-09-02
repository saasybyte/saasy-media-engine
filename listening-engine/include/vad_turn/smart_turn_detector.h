#pragma once

#include <onnxruntime_cxx_api.h>

#include <cstddef>
#include <string>
#include <vector>

namespace saasy::listening_engine {

struct TurnDetectorConfig {
  float turn_threshold = 0.5f; // Turn completion confidence threshold [0.0, 1.0]
  int n_threads_for_mel = 1; // Threads for mel spectrogram computation
};

/**
 * SmartTurnDetector wraps the Smart Turn v3 ONNX model for turn completion detection.
 * 
 * Analyzes complete conversational turns (0-8 seconds @ 16kHz) to determine if user has
 * finished speaking. Uses mel spectrogram features extracted via LogMelSpectrogram.
 * 
 * Stateless: Each call analyzes one turn independently.
 * Thread-safe for single producer.
 */
class SmartTurnDetector {
 public:
  explicit SmartTurnDetector(const std::string& model_path, 
                             const TurnDetectorConfig& config = TurnDetectorConfig());

  ~SmartTurnDetector();

  SmartTurnDetector(const SmartTurnDetector&) = delete;

  SmartTurnDetector& operator=(const SmartTurnDetector&) = delete;

  SmartTurnDetector(SmartTurnDetector&&) = delete;

  SmartTurnDetector& operator=(SmartTurnDetector&&) = delete;

  /**
   * Analyze audio buffer and return turn completion probability.
   * 
   * Input audio will be truncated to 8 seconds if longer (keeping most recent).
   * Audio is converted to mel spectrogram before ONNX inference.
   * 
   * @param audio_buffer 16kHz mono float PCM audio samples
   * @return Turn completion probability [0.0, 1.0], or -1.0f on error
   */
  float Process(const std::vector<float>& audio_buffer);

  /**
   * Check if the last processed probability indicates turn completion.
   * 
   * @param probability Turn completion probability from Process()
   * @return true if probability exceeds configured threshold
   */
  bool IsTurnComplete(float probability) const {
    return probability >= config_.turn_threshold;
  }

 private:
  static constexpr size_t kSampleRate = 16000;
  static constexpr size_t kMaxDurationSec = 8;
  static constexpr size_t kMaxSamples = kSampleRate * kMaxDurationSec;  // 128,000

  TurnDetectorConfig config_;
  Ort::Env env_;
  Ort::Session session_;
  Ort::MemoryInfo memory_info_; // CPU memory configuration: allocator strategy and memory type for tensor management
  Ort::AllocatorWithDefaultOptions allocator_;

  void InitializeOnnxSession(const std::string& model_path);
};

}  // namespace saasy::listening_engine
