#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Forward declaration
typedef struct SRC_STATE_tag SRC_STATE;

namespace saasy::listening_engine {

/**
 * AudioResampler converts audio from 48kHz 16-bit PCM to 16kHz 32-bit float PCM.
 * 
 * Input:  48kHz, mono, 16-bit PCM (int16_t), 480 samples @ 48kHz (10ms)
 * Output: 16kHz, mono, 32-bit float PCM normalized to [-1.0, 1.0], 160 samples @ 16kHz (10ms)
 * 
 * Thread-safe for single producer/consumer.
 */
class AudioResampler {
 public:
  AudioResampler();

  ~AudioResampler();

  AudioResampler(const AudioResampler&) = delete;

  AudioResampler& operator=(const AudioResampler&) = delete;

  AudioResampler(AudioResampler&&) = delete;

  AudioResampler& operator=(AudioResampler&&) = delete;

  bool Resample(const int16_t* input_audio, size_t input_frame_count, std::vector<float>& output_audio);

 private:
  static constexpr int kInputSampleRate = 48000;
  static constexpr int kOutputSampleRate = 16000;
  static constexpr double kResampleRatio = static_cast<double>(kOutputSampleRate) / kInputSampleRate;

  SRC_STATE* src_state_;
  std::vector<float> input_buffer_;
};

}  // namespace saasy::listening_engine
