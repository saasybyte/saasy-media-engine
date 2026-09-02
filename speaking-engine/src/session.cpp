#include "session.h"

#include <iostream>

#include "custom_audio_device_module.h"
#include "event_stream.h"
#include "media_stream.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/audio_options.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc_factory.h"

namespace saasy::speaking_engine {

Session::Session(const std::string& id)
    : session_id(id),
      participant_id(""),
      command_processor(std::make_unique<common::CommandProcessor>(id)),
      streams(std::make_unique<Streams>(id)) {

  command_processor->Start();

  // Create a TaskQueueFactory and keep it alive for the ADM/AudioDeviceBuffer.
  task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();

  // Create our Custom ADM (pass the factory).
  adm = webrtc::make_ref_counted<CustomAudioDeviceModule>(task_queue_factory_.get());

  // Allocate a lockfree SPSC queue for PCM chunks (capacity ~256 frames of 20ms)
  audio_queue = std::make_unique<boost::lockfree::spsc_queue<std::vector<uint8_t>>>(256);
  adm->SetAudioQueue(audio_queue.get());

  // Create PeerConnectionFactory with custom ADM
  peer_connection_factory = saasy::common::WebRTCFactory::CreateFactory(adm);
  if (!peer_connection_factory) {
    std::cerr << "[Session] Failed to create PeerConnectionFactory\n";
    return;
  }

  // Create a standard WebRTC audio source/track
  webrtc::AudioOptions audio_opts;
  audio_source = peer_connection_factory->CreateAudioSource(audio_opts);
  std::string track_id = "audio-" + session_id;
  audio_track = peer_connection_factory->CreateAudioTrack(track_id, audio_source.get());

  if (!audio_track) {
    std::cerr << "[Session] Failed to create audio track\n";
    return;
  }

  // Start the ADM capture side
  adm->Init();
  adm->InitRecording();
  adm->StartRecording();

  // Set playback completion callback
  adm->SetPlaybackCompletionCallback([this]() {
    std::cout << "[Session] Playback completion callback fired\n";

    playback_complete_.store(false);
    speech_generation_complete_.store(false);

    if (streams && streams->GetEventStream()) {
      streams->GetEventStream()->SendOnPlaybackCompleteEvent();
      std::cout << "[Session] OnPlaybackComplete event sent to Orchestrator\n";
    } else {
      std::cerr << "[Session] Event stream not available to send playback complete\n";
    }
  });

  std::cout << "[Session] Created session: " << session_id << " with audio track: " << track_id
            << "\n";
}

Session::~Session() {
  Cleanup();
  
  std::cout << "[Session] Destroyed session: " << session_id << "\n";
}

void Session::Cleanup() {
  if (cleaned_up_.exchange(true)) {
    return;
  }

  if (streams) {
    streams->StopAllStreams();
  }

  if (transport) {
    transport->Close();
  }

  audio_track = nullptr;
  if (adm) {
    adm->StopRecording();
    adm->Terminate();
    adm = nullptr;
  }
  audio_queue.reset();

  if (command_processor) {
    command_processor->Stop();
  }

  std::cout << "[Session] Cleaned up session: " << session_id << "\n";
}

void Session::SetRemovalCallback(std::function<void(const std::string&)> callback) {
  on_removal_callback_ = std::move(callback);
}

void Session::RequestRemoval() {
  if (removal_requested_.exchange(true)) {
    return;
  }
  
  std::cout << "[Session] Removal requested for session: " << session_id << "\n";
  
  if (on_removal_callback_) {
    on_removal_callback_(session_id);
  }
}

bool Session::IsRemovalRequested() const {
  return removal_requested_.load();
}

}  // namespace saasy::speaking_engine
