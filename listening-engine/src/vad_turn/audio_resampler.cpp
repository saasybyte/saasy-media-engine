#include "vad_turn/audio_resampler.h"

#include <iostream>
#include <samplerate.h>

namespace saasy::listening_engine {

AudioResampler::AudioResampler() {
  int error = 0;
  // SRC_SINC_FASTEST resampling algorithm provides good quality with low latency
  src_state_ = src_new(SRC_SINC_FASTEST, 1, &error);
  
  if (!src_state_) {
    std::cerr << "[AudioResampler] Failed to create resampler: " << src_strerror(error) << "\n";
  } else {
    std::cout << "[AudioResampler] Resampler created (48kHz → 16kHz, int16 → float32)\n";
  }
}

AudioResampler::~AudioResampler() {
  if (src_state_) {
    src_delete(src_state_);
    std::cout << "[AudioResampler] Destroyed\n";
  }
}

bool AudioResampler::Resample(const int16_t* input_audio, size_t input_frame_count,
                              std::vector<float>& output_audio) {
  if (!src_state_ || !input_audio || input_frame_count == 0) {
    return false;
  }

  // Convert int16 to float32 normalized to [-1.0, 1.0]
  // Divide by 32768.0 (2^15, max absolute value for signed 16-bit)
  input_buffer_.resize(input_frame_count);
  for (size_t i = 0; i < input_frame_count; ++i) {
    input_buffer_[i] = static_cast<float>(input_audio[i]) / 32768.0f;
  }

  // Calculate expected output size after resampling
  // 480 samples @ 48kHz → 160 samples @ 16kHz (+ 1 for safety margin)
  size_t output_frame_count = static_cast<size_t>(input_frame_count * kResampleRatio) + 1;
  output_audio.resize(output_frame_count);

  // Prepare libsamplerate data structure
  SRC_DATA src_data;
  src_data.data_in = input_buffer_.data();
  src_data.input_frames = static_cast<long>(input_frame_count);
  src_data.data_out = output_audio.data();
  src_data.output_frames = static_cast<long>(output_frame_count);
  src_data.src_ratio = kResampleRatio;
  src_data.end_of_input = 0;  // Continuous stream

  // Perform resampling
  int error = src_process(src_state_, &src_data);
  if (error != 0) {
    std::cerr << "[AudioResampler] Resample failed: " << src_strerror(error) << "\n";
    return false;
  }

  // Resize output to actual generated frames
  output_audio.resize(static_cast<size_t>(src_data.output_frames_gen));

  return true;
}

}  // namespace saasy::listening_engine
