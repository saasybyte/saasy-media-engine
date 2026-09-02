#pragma once

#include <atomic>
#include <functional>
#include <mediasoupclient/Device.hpp>
#include <mediasoupclient/Transport.hpp>
#include <memory>

#include "command_framework.h"
#include "custom_audio_device_module.h"
#include "event_stream.h"
#include "streams.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/peer_connection_interface.h"

namespace saasy::speaking_engine {

struct Session {
  std::string session_id;
  std::string participant_id;
  std::unique_ptr<common::CommandProcessor> command_processor;
  std::unique_ptr<Streams> streams;
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
  webrtc::scoped_refptr<CustomAudioDeviceModule> adm;
  std::unique_ptr<boost::lockfree::spsc_queue<std::vector<uint8_t>>> audio_queue;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
  webrtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source;
  webrtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
  std::unique_ptr<mediasoupclient::Device> device;
  std::unique_ptr<mediasoupclient::SendTransport> transport;
  std::unique_ptr<TransportListener> transport_listener;
  std::unique_ptr<ProducerListener> producer_listener;
  std::string audio_producer_id;
  std::atomic<bool> speech_generation_complete_{false};
  std::atomic<bool> playback_complete_{false};
  std::atomic<bool> removal_requested_{false};
  std::function<void(const std::string&)> on_removal_callback_;
  std::atomic<bool> cleaned_up_{false};

  Session(const std::string& id);

  ~Session();

  void Cleanup();

  void SetRemovalCallback(std::function<void(const std::string&)> callback);

  void RequestRemoval();

  bool IsRemovalRequested() const;
};

}  // namespace saasy::speaking_engine
