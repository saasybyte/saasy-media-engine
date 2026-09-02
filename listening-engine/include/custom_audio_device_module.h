#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/synchronization/mutex.h"

namespace saasy::listening_engine {

class CustomAudioDeviceModule : public webrtc::AudioDeviceModule {
 public:
  static constexpr int kSampleRateHz = 48000;
  static constexpr size_t kNumChannels = 1;
  static constexpr size_t kSamplesPer10ms = kSampleRateHz / 100;  // 480 @ 48k
  static constexpr size_t kBytesPerSample = 2;                    // PCM16

  explicit CustomAudioDeviceModule(webrtc::TaskQueueFactory* task_queue_factory);

  ~CustomAudioDeviceModule() override;

  int32_t ActiveAudioLayer(AudioLayer* audio_layer) const override;

  int32_t RegisterAudioCallback(webrtc::AudioTransport* audio_callback) override;

  int32_t Init() override;

  int32_t Terminate() override;

  bool Initialized() const override;

  int16_t PlayoutDevices() override { return 1; }

  int16_t RecordingDevices() override { return 0; }

  int32_t PlayoutDeviceName(uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
                            char guid[webrtc::kAdmMaxGuidSize]) override;

  int32_t RecordingDeviceName(uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override;

  int32_t SetPlayoutDevice(uint16_t /*index*/) override { return 0; }

  int32_t SetRecordingDevice(uint16_t /*index*/) override { return 0; }

  int32_t SetPlayoutDevice(WindowsDeviceType /*device*/) override { return 0; }

  int32_t SetRecordingDevice(WindowsDeviceType /*device*/) override { return 0; }

  int32_t PlayoutIsAvailable(bool* available) override;

  int32_t RecordingIsAvailable(bool* available) override;

  int32_t InitPlayout() override;

  int32_t InitRecording() override { return 0; }

  bool PlayoutIsInitialized() const override { return playout_is_initialized_; }

  bool RecordingIsInitialized() const override { return false; }

  int32_t StartPlayout() override;

  int32_t StopPlayout() override;

  bool Playing() const override { return playing_; }

  int32_t StartRecording() override { return 0; }

  int32_t StopRecording() override { return 0; }

  bool Recording() const override { return false; }

  int32_t InitSpeaker() override { return 0; }

  bool SpeakerIsInitialized() const override { return true; }

  int32_t InitMicrophone() override { return 0; }

  bool MicrophoneIsInitialized() const override { return false; }

  int32_t PlayoutDelay(uint16_t* delay_ms) const override {
    *delay_ms = 0;
    return 0;
  }

  bool BuiltInAECIsAvailable() const override { return false; }

  int32_t EnableBuiltInAEC(bool) override { return -1; }

  bool BuiltInAGCIsAvailable() const override { return false; }

  int32_t EnableBuiltInAGC(bool) override { return -1; }

  bool BuiltInNSIsAvailable() const override { return false; }

  int32_t EnableBuiltInNS(bool) override { return -1; }

  int32_t SpeakerVolumeIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }

  int32_t SetSpeakerVolume(uint32_t) override { return -1; }

  int32_t SpeakerVolume(uint32_t*) const override { return -1; }

  int32_t MaxSpeakerVolume(uint32_t*) const override { return -1; }

  int32_t MinSpeakerVolume(uint32_t*) const override { return -1; }

  int32_t MicrophoneVolumeIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }

  int32_t SetMicrophoneVolume(uint32_t) override { return -1; }

  int32_t MicrophoneVolume(uint32_t*) const override { return -1; }

  int32_t MaxMicrophoneVolume(uint32_t*) const override { return -1; }

  int32_t MinMicrophoneVolume(uint32_t*) const override { return -1; }

  int32_t SpeakerMuteIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }

  int32_t SetSpeakerMute(bool) override { return -1; }

  int32_t SpeakerMute(bool*) const override { return -1; }

  int32_t MicrophoneMuteIsAvailable(bool* available) override {
    *available = false;
    return 0;
  }

  int32_t SetMicrophoneMute(bool) override { return -1; }

  int32_t MicrophoneMute(bool*) const override { return -1; }

  int32_t StereoPlayoutIsAvailable(bool* available) const override {
    *available = false;
    return 0;
  }

  int32_t SetStereoPlayout(bool enable) override { return enable ? -1 : 0; }

  int32_t StereoPlayout(bool* enabled) const override {
    *enabled = false;
    return 0;
  }

  int32_t StereoRecordingIsAvailable(bool* available) const override {
    *available = false;
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override { return enable ? -1 : 0; }

  int32_t StereoRecording(bool* enabled) const override {
    *enabled = false;
    return 0;
  }

 private:
  void RunPlayoutLoop();

  webrtc::AudioDeviceBuffer audio_device_buffer_;
  webrtc::AudioTransport* audio_transport_cb_ = nullptr;
  mutable webrtc::Mutex mutex_;
  std::thread playout_thread_;
  std::atomic<bool> initialized_{false};
  std::atomic<bool> playout_is_initialized_{false};
  std::atomic<bool> playing_{false};
};

}  // namespace saasy::listening_engine
