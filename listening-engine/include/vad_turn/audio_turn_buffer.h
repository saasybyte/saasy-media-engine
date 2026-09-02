#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace saasy::listening_engine {

/**
 * AudioTurnBuffer is a ring buffer for storing up to 8 seconds of 16kHz audio.
 * 
 * Accumulates audio during entire conversational turn for SmartTurn v3 analysis.
 * Persists across multiple speech_started/speech_ended cycles until turn confirmed complete.
 * 
 * Input:  16kHz, mono, 32-bit float PCM normalized to [-1.0, 1.0]
 * Capacity: 8 seconds = 128,000 samples
 * 
 * Thread-safe for single producer.
 */
class AudioTurnBuffer {
 public:
  AudioTurnBuffer();

  ~AudioTurnBuffer();

  AudioTurnBuffer(const AudioTurnBuffer&) = delete;

  AudioTurnBuffer& operator=(const AudioTurnBuffer&) = delete;

  AudioTurnBuffer(AudioTurnBuffer&&) = delete;

  AudioTurnBuffer& operator=(AudioTurnBuffer&&) = delete;

  /**
   * Append audio samples to the buffer.
   * If buffer exceeds max capacity, oldest samples are discarded (ring behavior).
   * 
   * @param samples Audio samples to append
   */
  void Append(const std::vector<float>& samples);

  /**
   * Get the current buffered audio.
   * Returns up to the most recent 8 seconds.
   * 
   * @return Reference to internal buffer (valid until next Append/Clear)
   */
  const std::vector<float>& GetBuffer() const;

  /**
   * Clear all buffered audio.
   * Called when turn is complete or on explicit clear command.
   */
  void Clear();

  /**
   * Get current buffer duration in milliseconds.
   * 
   * @return Duration of buffered audio in ms
   */
  uint32_t GetDurationMs() const;

  /**
   * Get current buffer size in samples.
   * 
   * @return Number of samples in buffer
   */
  size_t GetSampleCount() const { return buffer_.size(); }

  /**
   * Check if buffer is empty.
   * 
   * @return true if buffer contains no samples
   */
  bool IsEmpty() const { return buffer_.empty(); }

 private:
  static constexpr size_t kSampleRate = 16000;
  static constexpr size_t kMaxDurationSec = 8;
  static constexpr size_t kMaxSamples = kSampleRate * kMaxDurationSec; // 128,000 samples

  std::vector<float> buffer_;
};

}  // namespace saasy::listening_engine
