#include "custom_audio_device_module.h"

#include <chrono>
#include <cstring>
#include <thread>

#include "api/task_queue/task_queue_factory.h"

namespace webrtc {
class TaskQueueFactory;
}

namespace saasy::speaking_engine {

CustomAudioDeviceModule::CustomAudioDeviceModule(webrtc::TaskQueueFactory* task_queue_factory)
    : audio_device_buffer_(task_queue_factory) {}

CustomAudioDeviceModule::~CustomAudioDeviceModule() { Terminate(); }

void CustomAudioDeviceModule::SetAudioQueue(boost::lockfree::spsc_queue<PcmChunk>* q) {
  webrtc::MutexLock lock(&mutex_);
  audio_queue_ = q;
}

int32_t CustomAudioDeviceModule::ActiveAudioLayer(AudioLayer* audio_layer) const {
  if (!audio_layer) return -1;
  *audio_layer = AudioLayer::kDummyAudio;  // We’re not exposing a real HW layer.
  return 0;
}

int32_t CustomAudioDeviceModule::RegisterAudioCallback(webrtc::AudioTransport* audio_callback) {
  webrtc::MutexLock lock(&mutex_);
  audio_transport_cb_ = audio_callback;
  return audio_device_buffer_.RegisterAudioCallback(audio_callback);
}

int32_t CustomAudioDeviceModule::Init() {
  bool expected = false;
  if (!initialized_.compare_exchange_strong(expected, true)) {
    return 0;  // already initialized
  }
  // Nothing heavy to do here; we size buffers at InitRecording when we know cfg.
  ring_samples_.clear();
  underruns_.store(0);
  return 0;
}

int32_t CustomAudioDeviceModule::Terminate() {
  StopRecording();
  initialized_.store(false);
  ring_samples_.clear();
  return 0;
}

bool CustomAudioDeviceModule::Initialized() const { return initialized_; }

int32_t CustomAudioDeviceModule::PlayoutDeviceName(uint16_t index,
                                                   char name[webrtc::kAdmMaxDeviceNameSize],
                                                   char guid[webrtc::kAdmMaxGuidSize]) {
  if (index != 0 || !name || !guid) return -1;
  std::strncpy(name, "Custom Playout (stub)", webrtc::kAdmMaxDeviceNameSize);
  std::strncpy(guid, "custom-playout-guid", webrtc::kAdmMaxGuidSize);
  return 0;
}

int32_t CustomAudioDeviceModule::RecordingDeviceName(uint16_t index,
                                                     char name[webrtc::kAdmMaxDeviceNameSize],
                                                     char guid[webrtc::kAdmMaxGuidSize]) {
  if (index != 0 || !name || !guid) return -1;
  std::strncpy(name, "Custom Capture (queue)", webrtc::kAdmMaxDeviceNameSize);
  std::strncpy(guid, "custom-capture-guid", webrtc::kAdmMaxGuidSize);
  return 0;
}

int32_t CustomAudioDeviceModule::PlayoutIsAvailable(bool* available) {
  if (!available) return -1;
  *available = false;  // capture-only ADM
  return 0;
}

int32_t CustomAudioDeviceModule::RecordingIsAvailable(bool* available) {
  if (!available) return -1;
  *available = true;
  return 0;
}

int32_t CustomAudioDeviceModule::InitRecording() {
  if (!initialized_) return -1;
  if (rec_is_initialized_) return 0;

  // Match FileAudioDevice: declare capture format on the conveyor.
  audio_device_buffer_.SetRecordingSampleRate(kSampleRateHz);
  audio_device_buffer_.SetRecordingChannels(static_cast<int8_t>(kNumChannels));

  rec_is_initialized_.store(true);
  return 0;
}

int32_t CustomAudioDeviceModule::StartRecording() {
  if (!initialized_ || !rec_is_initialized_) return -1;
  if (recording_) return 0;

  // Start the AudioDeviceBuffer side first (mirrors ADM impl).
  audio_device_buffer_.StartRecording();

  recording_.store(true);
  capture_thread_ = std::thread(&CustomAudioDeviceModule::RunCaptureLoop, this);
  return 0;
}

int32_t CustomAudioDeviceModule::StopRecording() {
  bool was_recording = recording_.exchange(false);
  if (was_recording && capture_thread_.joinable()) {
    capture_thread_.join();
  }
  // Stop conveyor after thread finishes posting data.
  audio_device_buffer_.StopRecording();
  return 0;
}

void CustomAudioDeviceModule::RunCaptureLoop() {
  using clock = std::chrono::steady_clock;
  auto next_deadline = clock::now();

  const size_t hard_cap_samples =
      (kSampleRateHz / 1000) * ring_hard_cap_frames_ms_ * kNumChannels;  // ~200ms

  std::vector<int16_t> frame;  // exactly one 10ms frame per tick

  while (recording_) {
    next_deadline += std::chrono::milliseconds(10);
    std::this_thread::sleep_until(next_deadline);

    // Drain queue into ring buffer (lock-free queue, but ring_samples_ needs protection)
    {
      webrtc::MutexLock lock(&mutex_);
      
      if (audio_queue_) {
        PcmChunk chunk;
        while (audio_queue_->pop(chunk)) {
          if (!chunk.empty() && (chunk.size() % 2) == 0) {
            const size_t samples = chunk.size() / sizeof(int16_t);
            const int16_t* p = reinterpret_cast<const int16_t*>(chunk.data());
            ring_samples_.insert(ring_samples_.end(), p, p + samples);
          }
        }
      }

      // Enforce the latency cap
      if (ring_samples_.size() > hard_cap_samples) {
        const size_t drop = ring_samples_.size() - hard_cap_samples;
        ring_samples_.erase(ring_samples_.begin(), ring_samples_.begin() + drop);
      }

      // Build one 10ms frame
      frame.clear();
      frame.reserve(kSamplesPer10ms * kNumChannels);
      AssembleOne10msFrame(frame);
    }

    // Deliver frame outside the lock (audio_device_buffer_ has its own synchronization)
    int8_t* bytes = reinterpret_cast<int8_t*>(frame.data());
    audio_device_buffer_.SetRecordedBuffer(bytes, static_cast<uint32_t>(kSamplesPer10ms));
    audio_device_buffer_.DeliverRecordedData();

    // Check playback completion (needs lock for callback access)
    bool should_fire_callback = false;
    PlaybackCompletionCallback playback_completion_callback_copy;
    {
      webrtc::MutexLock lock(&mutex_);
      if (speech_generation_complete_.load() && ring_samples_.empty()) {
        speech_generation_complete_.store(false);
        if (playback_completion_callback_) {
          should_fire_callback = true;
          playback_completion_callback_copy = playback_completion_callback_;
        }
      }
    }
    
    // Fire callback outside lock to avoid deadlock
    if (should_fire_callback && playback_completion_callback_copy) {
      playback_completion_callback_copy();
    }
  }
}

bool CustomAudioDeviceModule::AssembleOne10msFrame(std::vector<int16_t>& out) {
  const size_t need = kSamplesPer10ms * kNumChannels;

  if (ring_samples_.size() >= need) {
    out.insert(out.end(), ring_samples_.begin(), ring_samples_.begin() + need);
    ring_samples_.erase(ring_samples_.begin(), ring_samples_.begin() + need);
    return true;
  }

  // Not enough data: output silence and count an underrun.
  out.assign(need, 0);
  underruns_.fetch_add(1, std::memory_order_relaxed);

  return false;
}

void CustomAudioDeviceModule::FlushAudioBuffer(uint32_t fade_duration_ms) {
  webrtc::MutexLock lock(&mutex_);

  if (!recording_) {
    // Not recording, nothing to flush
    return;
  }

  // If fade_duration_ms > 0, apply linear fade-out to ring_samples_
  if (fade_duration_ms > 0 && !ring_samples_.empty()) {
    // Calculate how many samples to fade
    const size_t fade_samples = (kSampleRateHz * fade_duration_ms / 1000) * kNumChannels;
    const size_t samples_to_fade = std::min(fade_samples, ring_samples_.size());

    // Apply linear fade from full volume to silence (1.0 to 0.0)
    for (size_t i = 0; i < samples_to_fade; ++i) {
      float fade_factor = 1.0f - (static_cast<float>(i) / static_cast<float>(samples_to_fade));
      ring_samples_[i] = static_cast<int16_t>(ring_samples_[i] * fade_factor);
    }

    // Clear everything after the fade
    if (ring_samples_.size() > samples_to_fade) {
      ring_samples_.erase(ring_samples_.begin() + samples_to_fade, ring_samples_.end());
    }
  } else {
    // Immediate flush: clear all buffered audio
    ring_samples_.clear();
  }

  // Also drain the lock-free queue
  if (audio_queue_) {
    PcmChunk chunk;
    while (audio_queue_->pop(chunk)) {
      // Discard all queued chunks
    }
  }
}

void CustomAudioDeviceModule::SetPlaybackCompletionCallback(PlaybackCompletionCallback callback) {
  webrtc::MutexLock lock(&mutex_);
  playback_completion_callback_ = std::move(callback);
}

void CustomAudioDeviceModule::SpeechGenerationComplete() {
  speech_generation_complete_.store(true);
}

}  // namespace saasy::speaking_engine
