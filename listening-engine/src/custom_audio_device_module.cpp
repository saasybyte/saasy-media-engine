#include "custom_audio_device_module.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "api/task_queue/task_queue_factory.h"

namespace webrtc {
class TaskQueueFactory;
}

namespace saasy::listening_engine {

CustomAudioDeviceModule::CustomAudioDeviceModule(webrtc::TaskQueueFactory* task_queue_factory)
    : audio_device_buffer_(task_queue_factory) {}

CustomAudioDeviceModule::~CustomAudioDeviceModule() { Terminate(); }

int32_t CustomAudioDeviceModule::ActiveAudioLayer(AudioLayer* audio_layer) const {
  if (!audio_layer) return -1;
  *audio_layer = AudioLayer::kDummyAudio;
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
  return 0;
}

int32_t CustomAudioDeviceModule::Terminate() {
  StopPlayout();
  initialized_.store(false);
  return 0;
}

bool CustomAudioDeviceModule::Initialized() const { return initialized_; }

int32_t CustomAudioDeviceModule::PlayoutDeviceName(uint16_t index,
                                                    char name[webrtc::kAdmMaxDeviceNameSize],
                                                    char guid[webrtc::kAdmMaxGuidSize]) {
  if (index != 0 || !name || !guid) return -1;
  std::strncpy(name, "Custom Playout (sink)", webrtc::kAdmMaxDeviceNameSize);
  std::strncpy(guid, "custom-playout-guid", webrtc::kAdmMaxGuidSize);
  return 0;
}

int32_t CustomAudioDeviceModule::RecordingDeviceName(uint16_t index,
                                                      char name[webrtc::kAdmMaxDeviceNameSize],
                                                      char guid[webrtc::kAdmMaxGuidSize]) {
  if (index != 0 || !name || !guid) return -1;
  std::strncpy(name, "Custom Recording (stub)", webrtc::kAdmMaxDeviceNameSize);
  std::strncpy(guid, "custom-recording-guid", webrtc::kAdmMaxGuidSize);
  return 0;
}

int32_t CustomAudioDeviceModule::PlayoutIsAvailable(bool* available) {
  if (!available) return -1;
  *available = true;
  return 0;
}

int32_t CustomAudioDeviceModule::RecordingIsAvailable(bool* available) {
  if (!available) return -1;
  *available = false;  // playout-only ADM
  return 0;
}

int32_t CustomAudioDeviceModule::InitPlayout() {
  if (!initialized_) return -1;
  if (playout_is_initialized_) return 0;

  audio_device_buffer_.SetPlayoutSampleRate(kSampleRateHz);
  audio_device_buffer_.SetPlayoutChannels(static_cast<int8_t>(kNumChannels));

  playout_is_initialized_.store(true);
  return 0;
}

int32_t CustomAudioDeviceModule::StartPlayout() {
  if (!initialized_ || !playout_is_initialized_) return -1;
  if (playing_) return 0;

  audio_device_buffer_.StartPlayout();

  playing_.store(true);
  playout_thread_ = std::thread(&CustomAudioDeviceModule::RunPlayoutLoop, this);
  return 0;
}

int32_t CustomAudioDeviceModule::StopPlayout() {
  bool was_playing = playing_.exchange(false);
  if (was_playing && playout_thread_.joinable()) {
    playout_thread_.join();
  }
  audio_device_buffer_.StopPlayout();
  return 0;
}

void CustomAudioDeviceModule::RunPlayoutLoop() {
  using clock = std::chrono::steady_clock;
  auto next_deadline = clock::now();

  // Buffer to receive mixed playout data (discarded — audio is captured at track level)
  std::vector<int16_t> discard_buffer(kSamplesPer10ms * kNumChannels);

  while (playing_) {
    next_deadline += std::chrono::milliseconds(10);
    std::this_thread::sleep_until(next_deadline);

    // Drive the audio pipeline: pull decoded audio through the mixer/decoder chain.
    // This is what triggers AudioTrackSink::OnData() for consumers.
    audio_device_buffer_.RequestPlayoutData(kSamplesPer10ms);
    audio_device_buffer_.GetPlayoutData(discard_buffer.data());
  }
}

}  // namespace saasy::listening_engine
