#pragma once

#include "modules/audio_device/include/audio_device.h"

namespace saasy::common {

// Minimal no-op AudioDeviceModule for headless environments.
// Used only as a throwaway ADM for Device::Load() so mediasoup-client's
// internal factory can initialize without real audio hardware.
// No threads, no buffers — just satisfies Init()/Terminate().
class FakeAudioDeviceModule : public webrtc::AudioDeviceModule {
 public:
  FakeAudioDeviceModule() = default;
  ~FakeAudioDeviceModule() override = default;

  int32_t ActiveAudioLayer(AudioLayer* audio_layer) const override {
    *audio_layer = kDummyAudio;
    return 0;
  }

  int32_t RegisterAudioCallback(webrtc::AudioTransport*) override { return 0; }

  int32_t Init() override { return 0; }
  int32_t Terminate() override { return 0; }
  bool Initialized() const override { return true; }

  int16_t PlayoutDevices() override { return 1; }
  int16_t RecordingDevices() override { return 1; }

  int32_t PlayoutDeviceName(uint16_t, char name[webrtc::kAdmMaxDeviceNameSize],
                            char guid[webrtc::kAdmMaxGuidSize]) override {
    name[0] = '\0';
    guid[0] = '\0';
    return 0;
  }

  int32_t RecordingDeviceName(uint16_t, char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override {
    name[0] = '\0';
    guid[0] = '\0';
    return 0;
  }

  int32_t SetPlayoutDevice(uint16_t) override { return 0; }
  int32_t SetRecordingDevice(uint16_t) override { return 0; }
  int32_t SetPlayoutDevice(WindowsDeviceType) override { return 0; }
  int32_t SetRecordingDevice(WindowsDeviceType) override { return 0; }

  int32_t PlayoutIsAvailable(bool* available) override {
    *available = true;
    return 0;
  }

  int32_t RecordingIsAvailable(bool* available) override {
    *available = true;
    return 0;
  }

  int32_t InitPlayout() override { return 0; }
  int32_t InitRecording() override { return 0; }
  bool PlayoutIsInitialized() const override { return true; }
  bool RecordingIsInitialized() const override { return true; }
  int32_t StartPlayout() override { return 0; }
  int32_t StopPlayout() override { return 0; }
  bool Playing() const override { return false; }
  int32_t StartRecording() override { return 0; }
  int32_t StopRecording() override { return 0; }
  bool Recording() const override { return false; }

  int32_t InitSpeaker() override { return 0; }
  bool SpeakerIsInitialized() const override { return true; }
  int32_t InitMicrophone() override { return 0; }
  bool MicrophoneIsInitialized() const override { return true; }

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
};

}  // namespace saasy::common
