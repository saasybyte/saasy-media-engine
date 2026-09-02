#include "audio_track_sink.h"

#include <chrono>
#include <cstring>
#include <iostream>

namespace saasy::listening_engine {

AudioTrackSink::AudioTrackSink() { std::cout << "[AudioTrackSink] Created\n"; }

void AudioTrackSink::OnData(const void* audio_data, int bits_per_sample, int sample_rate,
                       size_t number_of_channels, size_t number_of_frames) {
  if (!running_) {
    return;
  }

  // WARN: Stereo should never arrive if upstream is configured correctly
  if (number_of_channels == 2) {
    static bool warned_once = false;
    if (!warned_once) {
      std::cerr << "[AudioTrackSink] WARNING: Received stereo audio - expected mono. "
                << "Check upstream configuration (web client opusStereo, channelCount).\n";
      warned_once = true;
    }
    return;  // Drop the frame entirely
  }

  // Calculate size of audio data
  size_t bytes_per_sample = bits_per_sample / 8;
  size_t total_bytes = number_of_frames * number_of_channels * bytes_per_sample;

  AudioFrame frame;
  frame.data.resize(total_bytes);
  std::memcpy(frame.data.data(), audio_data, total_bytes);
  frame.sample_rate = static_cast<uint32_t>(sample_rate);
  frame.channels = static_cast<uint32_t>(number_of_channels);
  frame.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

  // Push to queue (non-blocking)
  if (!audio_queue_.push(std::move(frame))) {
    std::cerr << "[AudioTrackSink] Audio queue full, dropping frame\n";
  }

  // Send to VadTurnPipeline (parallel processing)
  if (vad_turn_pipeline_) {
    // WebRTC gives us 48kHz, 16-bit PCM, mono
    const int16_t* audio_samples = static_cast<const int16_t*>(audio_data);
    vad_turn_pipeline_->ProcessAudio(audio_samples, number_of_frames);
  }
}

bool AudioTrackSink::GetNextFrame(AudioFrame& frame) {
  return audio_queue_.pop(frame); // attempts to remove the oldest element and assign it to 'frame'
}

void AudioTrackSink::Start() {
  if (running_.exchange(true)) {
    return;
  }
  std::cout << "[AudioTrackSink] Started\n";
}

void AudioTrackSink::Stop() {
  if (!running_.exchange(false)) {
    return;
  }

  AudioFrame frame;
  while (audio_queue_.pop(frame)) {
    // Discard
  }

  std::cout << "[AudioTrackSink] Stopped\n";
}

}  // namespace saasy::listening_engine
