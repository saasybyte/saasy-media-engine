#pragma once

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

#include "vad_turn/vad_turn_pipeline.h"
#include "webrtc/api/media_stream_interface.h"

namespace saasy::listening_engine {

// Forward declaration
class VadTurnPipeline;

struct AudioFrame {
  std::vector<uint8_t> data;
  uint32_t sample_rate;
  uint32_t channels;
  uint64_t timestamp_ms;
};

class AudioTrackSink : public webrtc::AudioTrackSinkInterface {
 public:
  AudioTrackSink();

  ~AudioTrackSink() override = default;

  void OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames) override;

  bool GetNextFrame(AudioFrame& frame);

  void Start();

  void Stop();

  bool IsRunning() const { return running_; }

  void SetVadTurnPipeline(VadTurnPipeline* pipeline) {
    vad_turn_pipeline_ = pipeline;
  }

 private:
  std::atomic<bool> running_{false};
  boost::lockfree::spsc_queue<AudioFrame> audio_queue_{256}; // Configurable size
  VadTurnPipeline* vad_turn_pipeline_ = nullptr;
};

}  // namespace saasy::listening_engine
