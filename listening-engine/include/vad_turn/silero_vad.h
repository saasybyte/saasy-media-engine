#pragma once

#include <onnxruntime_cxx_api.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace saasy::listening_engine {

enum class VadState {
  kIdle, // No speech detected
  kSpeech, // Speech detected
  kSilence, // Brief silence DURING speech
};

struct VadConfig {
  float threshold = 0.5f; // Speech probability threshold [0.0, 1.0]
  uint32_t min_speech_duration_ms = 50; // Min speech duration before speech_started
  uint32_t min_silence_duration_ms = 300; // Min silence duration before speech_ended
};

/**
 * SileroVAD wraps the Silero VAD ONNX model for real-time speech detection.
 * 
 * Processes 512-sample chunks @ 16kHz (32ms) and outputs speech probability.
 * Maintains RNN state between chunks for continuous stream processing.
 * 
 * Thread-safe for single producer.
 */
class SileroVAD {
 public:
  explicit SileroVAD(const std::string& model_path, const VadConfig& config = VadConfig());

  ~SileroVAD();

  SileroVAD(const SileroVAD&) = delete;

  SileroVAD& operator=(const SileroVAD&) = delete;

  SileroVAD(SileroVAD&&) = delete;

  SileroVAD& operator=(SileroVAD&&) = delete;

  /**
   * Process a chunk of audio and return speech probability.
   * 
   * @param audio_chunk 512 samples of 32-bit float audio @ 16kHz
   * @return Speech probability [0.0, 1.0], or -1.0f on error
   */
  float Process(const std::vector<float>& audio_chunk);

  /**
   * Reset internal RNN state. Call when starting a new audio stream.
   */
  void Reset();

  VadState GetState() const { return state_; }

 private:
  static constexpr uint32_t kChunkDurationMs = 32;  // 512 samples @ 16kHz
  static constexpr size_t kChunkSize = 512; // 32ms @ 16kHz
  static constexpr size_t kSampleRate = 16000;
  static constexpr size_t kStateDim = 128; // Silero RNN state dimension
  static constexpr size_t kContextSamples = 64; // Context from previous chunk (v6: managed by us)
  static constexpr size_t kEffectiveInputSize = kChunkSize + kContextSamples; // 576

  VadConfig config_;
  VadState state_;
  uint32_t speech_duration_ms_;
  uint32_t silence_duration_ms_;
  Ort::Env env_;
  Ort::Session session_;
  Ort::MemoryInfo memory_info_; // CPU memory configuration: allocator strategy and memory type for tensor management
  Ort::AllocatorWithDefaultOptions allocator_;
  std::vector<float> state_tensor_; // RNN hidden state [2, 1, 128]
  std::vector<float> context_buffer_; // Last 64 samples from previous chunk (v6: managed by us, not model)
  int64_t sample_rate_tensor_; // Sample rate value

  void InitializeOnnxSession(const std::string& model_path);

  void UpdateState(float probability);
};

}  // namespace saasy::listening_engine
