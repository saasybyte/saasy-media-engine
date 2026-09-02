#include "streams.h"

#include <iostream>

namespace saasy::listening_engine {

Streams::Streams(const std::string& session_id)
    : session_id_(session_id) {
  std::cout << "[Streams] Created for session: " << session_id << "\n";
}

Streams::~Streams() {
  StopAllStreams();
  std::cout << "[Streams] Destroyed for session: " << session_id_ << "\n";
}

void Streams::SetControlStream(std::unique_ptr<ControlStream> stream) {
  control_stream_ = std::move(stream);
}

void Streams::SetEventStream(std::unique_ptr<EventStream> stream) {
  event_stream_ = std::move(stream);
}

void Streams::SetMediaStream(std::unique_ptr<MediaStream> stream) {
  media_stream_ = std::move(stream);
}

void Streams::StopAllStreams() {
  if (control_stream_) {
    control_stream_->Stop();
  }
  if (event_stream_) {
    event_stream_->Stop();
  }
  if (media_stream_) {
    media_stream_->Stop();
  }
}

}  // namespace saasy::listening_engine
