#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>

#include "event_stream.h"
#include "media_stream.h"
#include "control_stream.h"

namespace saasy::speaking_engine {

class Streams{
 public:
  Streams(const std::string& session_id);

  ~Streams();

  void SetControlStream(std::unique_ptr<ControlStream> stream);

  void SetEventStream(std::unique_ptr<EventStream> stream);

  void SetMediaStream(std::unique_ptr<MediaStream> stream);

  ControlStream* GetControlStream() { return control_stream_.get(); }

  EventStream* GetEventStream() { return event_stream_.get(); }

  MediaStream* GetMediaStream() { return media_stream_.get(); }

  void StopAllStreams();

 private:
  std::string session_id_;
  std::unique_ptr<ControlStream> control_stream_;
  std::unique_ptr<EventStream> event_stream_;
  std::unique_ptr<MediaStream> media_stream_;
};

}  // namespace saasy::speaking_engine
