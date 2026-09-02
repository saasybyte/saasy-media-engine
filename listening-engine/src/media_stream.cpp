#include "media_stream.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "session.h"

namespace saasy::listening_engine {

MediaStream::MediaStream(
    const std::string& session_id, Session* session,
    grpc::ServerReaderWriter<v1::ListeningEngineMediaPayload, v1::ListeningEngineMediaAck>* stream)
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
            << "  Frames sent: " << stats_.frames_sent << "\n"
            << "  Frames dropped: " << stats_.frames_dropped << "\n"
            << "  Bytes sent: " << stats_.bytes_sent << "\n"
            << "  Average frame rate: " << std::fixed << std::setprecision(2)
            << stats_.GetFrameRate() << " fps\n";
}

void MediaStream::Start() {
  running_ = true;
  media_thread_ = std::thread(&MediaStream::ForwardMediaStream, this);
  std::cout << "[MediaStream] Started media stream forwarding for session: " << session_id_ << "\n";
}

void MediaStream::Stop() {
  running_ = false;
  std::cout << "[MediaStream] Stopping media stream for session: " << session_id_ << "\n";
}

void MediaStream::ForwardMediaStream() {
  AudioFrame frame;
  while (running_) {
    if (session_->audio_track_sink->GetNextFrame(frame)) {
      // Always add frame to pre-roll buffer
      {
        std::lock_guard<std::mutex> lock(pre_roll_mutex_);
        for (uint8_t byte : frame.data) {
          pre_roll_buffer_.push_back(byte);
        }
      }
      
      // Only forward if gate is open
      if (gate_open_.load(std::memory_order_relaxed)) {
        std::string consumer_id = session_->GetFirstConsumerId();

        if (!consumer_id.empty()) {
          SendMediaFrame(consumer_id, frame.data, frame.sample_rate,
                        frame.channels, frame.timestamp_ms);
        }
      }
      // Else: gate closed, frame dropped (silence suppression)
      
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  std::cout << "[MediaStream] Media forwarding ended for session: " << session_id_ << "\n";
}

void MediaStream::SendMediaFrame(const std::string& device_consumer_id,
                                 const std::vector<uint8_t>& frame_data, uint32_t sample_rate,
                                 uint32_t channels, uint64_t timestamp_ms) {
  if (!running_) {
    return;
  }

  // Validate frame size
  if (frame_data.size() > kMaxFrameSize) {
    stats_.frames_dropped++;
    std::cerr << "[MediaStream] Frame too large: " << frame_data.size()
              << " bytes (max: " << kMaxFrameSize << ")\n";
    return;
  }

  // Create payload with media frame
  v1::ListeningEngineMediaPayload payload;
  payload.set_type("media_frame");
  payload.set_request_id(GenerateRequestId());
  payload.set_session_id(session_id_);
  payload.set_participant_id(session_->participant_id);

  // Fill in media frame
  auto* media_frame = payload.mutable_media_frame();
  media_frame->set_device_consumer_id(device_consumer_id);
  media_frame->set_kind(saasy::shared::v1::MEDIA_KIND_AUDIO);
  media_frame->set_frame_data(frame_data.data(), frame_data.size());
  media_frame->set_sample_rate(sample_rate);
  media_frame->set_channels(channels);
  media_frame->set_timestamp_ms(timestamp_ms);

  // Send the frame
  std::lock_guard<std::mutex> lock(write_mutex_);
  if (!stream_->Write(payload)) {
    stats_.frames_dropped++;
    std::cerr << "[MediaStream] Failed to send media frame for session: " << session_id_ << "\n";
    running_ = false;
  } else {
    stats_.frames_sent++;
    stats_.bytes_sent += frame_data.size();

    // Log statistics at intervals
    if (stats_.frames_sent % kLogInterval == 0) {
      std::cout << "[MediaStream] Session " << session_id_
                << " - Frames sent: " << stats_.frames_sent
                << " (dropped: " << stats_.frames_dropped << ")"
                << ", Rate: " << std::fixed << std::setprecision(2) << stats_.GetFrameRate()
                << " fps\n";
    }
  }
}

std::string MediaStream::GenerateRequestId() {
  std::stringstream ss;
  ss << session_id_ << "_media_" << request_counter_.fetch_add(1);
  return ss.str();
}

void MediaStream::OnVadTurnEvent(const std::string& event_type, [[maybe_unused]] uint64_t timestamp_ms) {
  if (event_type == "speech_started") {
    std::cout << "[MediaStream] Speech started - flushing pre-roll buffer and opening gate for session: " 
              << session_id_ << "\n";
    FlushPreRollBuffer();
    gate_open_.store(true, std::memory_order_relaxed);
    
  } else if (event_type == "user_turn_complete") {
    std::cout << "[MediaStream] User turn complete - closing gate for session: " 
              << session_id_ << "\n";
    gate_open_.store(false, std::memory_order_relaxed);
  }
}

void MediaStream::FlushPreRollBuffer() {
  std::vector<uint8_t> pre_roll_data;
  
  {
    std::lock_guard<std::mutex> lock(pre_roll_mutex_);
    if (pre_roll_buffer_.empty()) {
      std::cout << "[MediaStream] Pre-roll buffer empty, nothing to flush\n";
      return;
    }
    pre_roll_data.assign(pre_roll_buffer_.begin(), pre_roll_buffer_.end());
  }
  
  std::cout << "[MediaStream] Flushing " << pre_roll_data.size() 
            << " bytes of pre-roll audio for session: " << session_id_ << "\n";
  
  // Send pre-roll as a single frame
  std::string consumer_id = session_->GetFirstConsumerId();
  
  if (!consumer_id.empty()) {
    uint64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    SendMediaFrame(consumer_id, pre_roll_data, 48000, 1, timestamp_ms);
  }
}

}  // namespace saasy::listening_engine
