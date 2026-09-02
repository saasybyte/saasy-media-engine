#pragma once

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <cstdint>
#include <deque>
#include <thread>
#include <vector>

#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/synchronization/mutex.h"

namespace saasy::speaking_engine {

class CustomAudioDeviceModule : public webrtc::AudioDeviceModule {
 public:
  // Source: queue of PCM16 frames (TTS producer pushes here).
  using PcmChunk = std::vector<uint8_t>;
  using PlaybackCompletionCallback = std::function<void()>;

  // Static capture format (matches WebRTC defaults; update if you negotiate stereo).
  static constexpr int kSampleRateHz = 48000;
  static constexpr size_t kNumChannels = 1;  // mono (set to 2 if stereo is negotiated)
  static constexpr size_t kSamplesPer10ms = kSampleRateHz / 100;  // 480 @ 48k
  static constexpr size_t kBytesPerSample = 2;                    // PCM16
  static constexpr size_t kBytesPer10msMono = kSamplesPer10ms * kBytesPerSample * 1;

  explicit CustomAudioDeviceModule(webrtc::TaskQueueFactory* task_queue_factory);

  ~CustomAudioDeviceModule() override;

  void SetAudioQueue(boost::lockfree::spsc_queue<PcmChunk>* q);

  void FlushAudioBuffer(uint32_t fade_duration_ms);

  void SetPlaybackCompletionCallback(PlaybackCompletionCallback callback);

  void SpeechGenerationComplete();

  int32_t ActiveAudioLayer(AudioLayer* audio_layer) const override;

  int32_t RegisterAudioCallback(webrtc::AudioTransport* audio_callback) override;

  int32_t Init() override;

  int32_t Terminate() override;

  bool Initialized() const override;

  int16_t PlayoutDevices() override { return 0; }

  int16_t RecordingDevices() override { return 1; }

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

  int32_t InitPlayout() override { return 0; }

  int32_t InitRecording() override;

  bool PlayoutIsInitialized() const override { return false; }

  bool RecordingIsInitialized() const override { return rec_is_initialized_; }

  int32_t StartPlayout() override { return 0; }

  int32_t StopPlayout() override { return 0; }

  bool Playing() const override { return false; }

  int32_t StartRecording() override;

  int32_t StopRecording() override;

  bool Recording() const override { return recording_; }

  int32_t InitSpeaker() override { return 0; }

  bool SpeakerIsInitialized() const override { return false; }

  int32_t InitMicrophone() override { return 0; }

  bool MicrophoneIsInitialized() const override { return true; }

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
    *available = (kNumChannels == 2);
    return 0;
  }

  int32_t SetStereoRecording(bool enable) override { return enable ? -1 : 0; }

  int32_t StereoRecording(bool* enabled) const override {
    *enabled = (kNumChannels == 2);
    return 0;
  }

 private:
  // Capture loop: single thread that ticks every 10 ms and pushes one frame.
  void RunCaptureLoop();

  bool AssembleOne10msFrame(std::vector<int16_t>& out_samples);  // pull from queue or fill silence

  boost::lockfree::spsc_queue<PcmChunk>* audio_queue_ = nullptr;
  std::deque<int16_t> ring_samples_;      // internal sample ring (int16_t units)
  size_t ring_hard_cap_frames_ms_ = 200;  // 200 ms hard cap
  webrtc::AudioDeviceBuffer audio_device_buffer_;
  webrtc::AudioTransport* audio_transport_cb_ = nullptr;
  mutable webrtc::Mutex mutex_;
  std::thread capture_thread_;
  std::atomic<bool> initialized_{false};
  std::atomic<bool> rec_is_initialized_{false};
  std::atomic<bool> recording_{false};
  std::atomic<uint64_t> underruns_{0};
  PlaybackCompletionCallback playback_completion_callback_;
  std::atomic<bool> speech_generation_complete_{false};
};

}  // namespace saasy::speaking_engine
