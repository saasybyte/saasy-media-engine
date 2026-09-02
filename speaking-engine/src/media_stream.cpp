#include "media_stream.h"

#include <iomanip>
#include <iostream>

#include "session.h"

namespace saasy::speaking_engine {

MediaStream::MediaStream(
    const std::string& session_id, Session* session,
    grpc::ServerReaderWriter<v1::SpeakingEngineMediaAck, v1::SpeakingEngineMediaPayload>* stream)
    : session_id_(session_id), session_(session), stream_(stream) {
  stats_.Reset();
  std::cout << "[MediaStream] Created media stream for session: " << session_id << "\n";
}

MediaStream::~MediaStream() {
  Stop();

  // It is mandatory to join threads before their std::thread object is destroyed.
  // This waits for the threads to finish their work and exit gracefully,
  // preventing std::terminate from being called and crashing the application.
  if (media_thread_.joinable()) {
    media_thread_.join();
  }

  // Log final statistics
  std::cout << "[MediaStream] Final stats for session " << session_id_ << ":\n"
            << "  Frames received: " << stats_.frames_received << "\n"
            << "  Frames dropped: " << stats_.frames_dropped << "\n"
            << "  Bytes received: " << stats_.bytes_received << "\n"
            << "  Average frame rate: " << std::fixed << std::setprecision(2)
            << stats_.GetFrameRate() << " fps\n";
}

void MediaStream::Start() {
  running_ = true;
  media_thread_ = std::thread(&MediaStream::ProcessMediaStream, this);
  std::cout << "[MediaStream] Started media stream processor for session: " << session_id_ << "\n";
}

void MediaStream::Stop() {
  running_ = false;
  std::cout << "[MediaStream] Stopping media stream for session: " << session_id_ << "\n";
}

void MediaStream::ProcessMediaStream() {
  v1::SpeakingEngineMediaPayload payload;
  std::string participant_id;

  while (running_ && stream_->Read(&payload)) {
    participant_id = payload.participant_id();

    // Validate session ID
    if (payload.session_id() != session_id_) {
      SendErrorResponse(payload.request_id(), participant_id, "INVALID_ARGUMENT",
                        "Session ID mismatch");
      continue;
    }

    // Handle media frame
    if (payload.has_media_frame()) {
      HandleMediaFrame(payload.media_frame(), payload.request_id());

      // Log statistics at intervals
      if (stats_.frames_received % kLogInterval == 0) {
        std::cout << "[MediaStream] Session " << session_id_
                  << " - Frames: " << stats_.frames_received
                  << " (dropped: " << stats_.frames_dropped << ")"
                  << ", Rate: " << std::fixed << std::setprecision(2) << stats_.GetFrameRate()
                  << " fps\n";
      }
    }
  }

  std::cout << "[MediaStream] Media stream processor ended for session: " << session_id_ << "\n";
}

void MediaStream::HandleMediaFrame(const v1::MediaFrame& frame,
                                   [[maybe_unused]] const std::string& request_id) {
  if (!ValidateMediaFrame(frame) || !session_->audio_track || !session_->adm || !session_->audio_queue) {
      stats_.frames_dropped++;
      return;
  }

  // Verify device_producer_id matches our producer
  if (!session_->audio_producer_id.empty() &&
      frame.device_producer_id() != session_->audio_producer_id) {
    stats_.frames_dropped++;
    return;
  }

  // Frame payload should be PCM16 mono @ 48kHz (20ms = 960 samples = 1920 bytes)
  const std::string& frame_data = frame.frame_data();
  const size_t byte_count = frame_data.size();

  if ((byte_count % 2) != 0) {
    stats_.frames_dropped++;
    return;
  }

  // Update stats
  stats_.bytes_received += byte_count;

  // Push into ADM queue (copy once into the SPSC queue)
  // If queue is full, we drop the chunk to preserve cadence.
  bool pushed = session_->audio_queue->push(std::vector<uint8_t>(frame_data.begin(),
                                                                 frame_data.end()));
  if (!pushed) {
    stats_.frames_dropped++;
  } else {
    stats_.frames_received++;
  }
}

bool MediaStream::ValidateMediaFrame(const v1::MediaFrame& frame) {
  // Check frame size
  if (frame.frame_data().size() > kMaxFrameSize) {
    std::cerr << "[MediaStream] Frame too large: " << frame.frame_data().size()
              << " bytes (max: " << kMaxFrameSize << ")\n";
    return false;
  }

  // Check frame is not empty
  if (frame.frame_data().empty()) {
    std::cerr << "[MediaStream] Empty frame received\n";
    return false;
  }

  // Check media kind
  if (frame.kind() != saasy::shared::v1::MEDIA_KIND_AUDIO &&
      frame.kind() != saasy::shared::v1::MEDIA_KIND_VIDEO) {
    std::cerr << "[MediaStream] Invalid media kind: " << frame.kind() << "\n";
    return false;
  }

  // Check device_producer_id is not empty
  if (frame.device_producer_id().empty()) {
    std::cerr << "[MediaStream] Missing device_producer_id\n";
    return false;
  }

  return true;
}

void MediaStream::SendErrorResponse(const std::string& request_id,
                                    const std::string& participant_id,
                                    const std::string& error_code,
                                    const std::string& error_message) {
  v1::SpeakingEngineMediaAck ack;
  ack.set_type("error");
  ack.set_request_id(request_id);
  ack.set_session_id(session_id_);
  ack.set_participant_id(participant_id);

  auto* error = ack.mutable_error_response();
  error->set_code(error_code);
  error->set_message(error_message);

  std::lock_guard<std::mutex> lock(write_mutex_);
  if (!stream_->Write(ack)) {
    std::cerr << "[MediaStream] Failed to send error response for session: " << session_id_ << "\n";
  }
}

}  // namespace saasy::speaking_engine
