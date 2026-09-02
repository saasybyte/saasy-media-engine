#include "vad_turn/audio_turn_buffer.h"

#include <iostream>

namespace saasy::listening_engine {

AudioTurnBuffer::AudioTurnBuffer() {
  buffer_.reserve(kMaxSamples);
  std::cout << "[AudioBuffer] Created (max capacity: " << kMaxDurationSec 
            << " seconds, " << kMaxSamples << " samples)\n";
}

AudioTurnBuffer::~AudioTurnBuffer() {
  std::cout << "[AudioTurnBuffer] Destroyed\n";
}

void AudioTurnBuffer::Append(const std::vector<float>& samples) {
  if (samples.empty()) {
    return;
  }

  // If adding these samples would exceed capacity, implement ring behavior
  if (buffer_.size() + samples.size() > kMaxSamples) {
    // Calculate how many samples to keep from existing buffer
    size_t samples_to_keep = kMaxSamples - samples.size();
    
    if (samples_to_keep > 0 && samples_to_keep < buffer_.size()) {
      // Shift in most recent samples, discard oldest
      std::vector<float> temp_buffer;
      temp_buffer.reserve(kMaxSamples);
      temp_buffer.insert(temp_buffer.end(), 
                        buffer_.end() - samples_to_keep, 
                        buffer_.end());
      buffer_ = std::move(temp_buffer);
    } else if (samples.size() >= kMaxSamples) {
      // New samples alone exceed capacity - shift in only the most recent
      buffer_.clear();
      buffer_.insert(buffer_.end(), 
                    samples.end() - kMaxSamples, 
                    samples.end());
      return;
    } else {
      buffer_.clear();
    }
  }

  // Append new samples
  buffer_.insert(buffer_.end(), samples.begin(), samples.end());
}

const std::vector<float>& AudioTurnBuffer::GetBuffer() const {
  return buffer_;
}

void AudioTurnBuffer::Clear() {
  buffer_.clear();
  std::cout << "[AudioTurnBuffer] Cleared\n";
}

uint32_t AudioTurnBuffer::GetDurationMs() const {
  if (buffer_.empty()) {
    return 0;
  }
  return static_cast<uint32_t>((buffer_.size() * 1000) / kSampleRate);
}

}  // namespace saasy::listening_engine
