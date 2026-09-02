#include "session.h"

#include <iostream>

#include "api/task_queue/default_task_queue_factory.h"
#include "custom_audio_device_module.h"
#include "event_stream.h"
#include "media_stream.h"
#include "webrtc_factory.h"

namespace saasy::listening_engine {

Session::Session(const std::string& id)
    : session_id(id),
      participant_id(""),
      command_processor(std::make_unique<common::CommandProcessor>(id)),
      streams(std::make_unique<Streams>(id)),
      vad_turn_pipeline(nullptr) {

  command_processor->Start();

  // Create a TaskQueueFactory and keep it alive for the ADM/AudioDeviceBuffer.
  task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();

  // Create our custom playout-only ADM (virtual speaker for headless environments).
  adm = webrtc::make_ref_counted<CustomAudioDeviceModule>(task_queue_factory_.get());

  // Create PeerConnectionFactory with custom ADM
  peer_connection_factory = saasy::common::WebRTCFactory::CreateFactory(adm);
  if (!peer_connection_factory) {
    std::cerr << "[Session] Failed to create PeerConnectionFactory\n";
    return;
  }

  // Start the ADM playout side (drives WebRTC's decoder pipeline)
  adm->Init();
  adm->InitPlayout();
  adm->StartPlayout();

  // Create audio sink
  audio_track_sink = std::make_unique<AudioTrackSink>();
  audio_track_sink->Start();

  std::cout << "[Session] Created session: " << session_id << "\n";
}

Session::~Session() {
  Cleanup();
  
  std::cout << "[Session] Destroyed session: " << session_id << "\n";
}

std::string Session::GetFirstConsumerId() const {
  std::lock_guard<std::mutex> lock(consumers_mutex_);
  if (consumers.empty()) {
    return "";
  }
  return consumers.begin()->first;
}

void Session::Cleanup() {
  if (cleaned_up_.exchange(true)) {
    return;
  }

  if (audio_track_sink) {
    audio_track_sink->Stop();
  }

  {
    std::lock_guard<std::mutex> lock(consumers_mutex_);
    for (const auto& [id, consumer] : consumers) {
      if (auto* track = consumer->GetTrack()) {
        if (auto* audio_track = static_cast<webrtc::AudioTrackInterface*>(track)) {
          audio_track->RemoveSink(audio_track_sink.get());
        }
      }
    }
  }

  if (vad_turn_pipeline) {
    vad_turn_pipeline->Stop();
  }

  if (streams) {
    streams->StopAllStreams();
  }

  {
    std::lock_guard<std::mutex> lock(consumers_mutex_);
    for (auto& [consumer_id, consumer] : consumers) {
      consumer->Close();
    }
    consumers.clear();
  }
  
  if (transport) {
    transport->Close();
  }

  if (adm) {
    adm->StopPlayout();
    adm->Terminate();
    adm = nullptr;
  }

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

}  // namespace saasy::listening_engine
