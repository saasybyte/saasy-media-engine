#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mediasoupclient/Consumer.hpp>
#include <mediasoupclient/Device.hpp>
#include <mediasoupclient/Transport.hpp>
#include <memory>
#include <mutex>

#include "audio_track_sink.h"
#include "command_framework.h"
#include "control_stream.h"
#include "custom_audio_device_module.h"
#include "event_stream.h"
#include "streams.h"
#include "vad_turn/vad_turn_pipeline.h"

namespace saasy::listening_engine {

struct Session {
  std::string session_id;
  std::string participant_id;
  std::unique_ptr<common::CommandProcessor> command_processor;
  std::unique_ptr<Streams> streams;
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
  webrtc::scoped_refptr<CustomAudioDeviceModule> adm;
  // think of AudioTrackSink as the media reciever, whereas the AudioTrack is the media broadcast
  std::unique_ptr<AudioTrackSink> audio_track_sink;
  std::unique_ptr<VadTurnPipeline> vad_turn_pipeline;
  std::unique_ptr<mediasoupclient::Device> device;
  std::unique_ptr<mediasoupclient::RecvTransport> transport;
  std::unique_ptr<TransportListener> transport_listener;
  mutable std::mutex consumers_mutex_;
  std::map<std::string, std::unique_ptr<mediasoupclient::Consumer>> consumers;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
  std::atomic<bool> removal_requested_{false};
  std::function<void(const std::string&)> on_removal_callback_;
  std::atomic<bool> cleaned_up_{false};

  Session(const std::string& id);

  ~Session();

  std::string GetFirstConsumerId() const;

  void Cleanup();

  void SetRemovalCallback(std::function<void(const std::string&)> callback);

  void RequestRemoval();

  bool IsRemovalRequested() const;
};

}  // namespace saasy::listening_engine
