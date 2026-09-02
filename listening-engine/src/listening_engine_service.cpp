#include "listening_engine_service.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "commands.h"
#include "event_stream.h"
#include "media_stream.h"
#include "vad_turn/log_mel_spectrogram.h"
#include "vad_turn/vad_turn_pipeline.h"

namespace saasy::listening_engine {

ListeningEngineServiceImpl::ListeningEngineServiceImpl(const ListeningEngineConfig& config)
    : config_(config) {
  std::cout << "[Service] Listening Engine Service initializing...\n";
  Initialize();
  std::cout << "[Service] Listening Engine Service initialized (kMaxSessions=" << kMaxSessions
            << ")\n";
}

ListeningEngineServiceImpl::~ListeningEngineServiceImpl() {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  sessions_.clear();
}

void ListeningEngineServiceImpl::Initialize() {
  // Initialize mel spectrogram lookup tables and filters once for application lifetime.
  // These static members persist in memory and are shared read-only across all
  // concurrent sessions for thread-safe mel spectrogram computation.
  if (!LogMelSpectrogram::Init(config_.mel_filters_path.c_str())) {
    std::cerr << "[Service] FATAL: Failed to load mel filters from: " << config_.mel_filters_path
              << "\n";
    throw std::runtime_error("Failed to load mel filters");
  }
  std::cout << "[Service] Loaded mel filters\n";

  try {
    shared_silero_vad_ = std::make_shared<SileroVAD>(config_.vad_model_path, config_.vad_config);
    std::cout << "[Service] Loaded Silero VAD model\n";
  } catch (const std::exception& e) {
    std::cerr << "[Service] FATAL: Failed to load VAD model: " << e.what() << "\n";
    throw;
  }

  try {
    shared_smart_turn_detector_ =
        std::make_shared<SmartTurnDetector>(config_.turn_model_path, config_.turn_config);
    std::cout << "[Service] Loaded Smart Turn v3 model\n";
  } catch (const std::exception& e) {
    std::cerr << "[Service] FATAL: Failed to load turn detector model: " << e.what() << "\n";
    throw;
  }
}

Session* ListeningEngineServiceImpl::GetOrCreateSession(const std::string& session_id,
                                                        const std::string& participant_id) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    it->second->participant_id = participant_id;
    return it->second.get();
  }

  if (sessions_.size() >= kMaxSessions) {
    std::cerr << "[Service] Session limit reached (" << kMaxSessions << ")\n";
    return nullptr;
  }

  auto session = std::make_unique<Session>(session_id);
  auto* session_ptr = session.get();
  
  // Set removal callback before storing in map
  session_ptr->SetRemovalCallback([this](const std::string& sid) {
    // We spawn a detached call to avoid deadlock since we might
    // already hold locks in the request handler.
    std::thread([this, sid]() {
      RemoveSession(sid);
    }).detach();
  });
  
  sessions_[session_id] = std::move(session);
  session_ptr->participant_id = participant_id;

  return session_ptr;
}

void ListeningEngineServiceImpl::RemoveSession(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);

  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return;
  }

  // Cleanup before destroying
  it->second->Cleanup();

  sessions_.erase(it);
  std::cout << "[Service] Removed session: " << session_id << "\n";
}

grpc::Status ListeningEngineServiceImpl::HealthCheck(grpc::ServerContext* /*context*/,
                                                     const v1::HealthCheckRequest* /*request*/,
                                                     v1::HealthCheckResponse* response) {
  response->set_alive(true);
  return grpc::Status::OK;
}

grpc::Status ListeningEngineServiceImpl::Control(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<v1::ListeningEngineControlMessage, v1::ListeningEngineControlMessage>*
        stream) {
  std::cout << "[Service] Control stream established\n";

  // Extract session_id from metadata
  std::string session_id;
  std::string participant_id;
  const auto& metadata = context->client_metadata();

  auto session_it = metadata.find("session-id");
  if (session_it != metadata.end()) {
    session_id = std::string(session_it->second.data(), session_it->second.length());
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "'session-id' metadata is required");
  }

  auto participant_it = metadata.find("participant-id");
  if (participant_it != metadata.end()) {
    participant_id = std::string(participant_it->second.data(), participant_it->second.length());
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "'participant-id' metadata is required");
  }

  std::cout << "[Service] Control stream for session: " << session_id << "\n";

  // Get or create session
  auto* session = GetOrCreateSession(session_id, participant_id);
  if (!session) {
    v1::ListeningEngineControlMessage error_response;
    error_response.set_direction(v1::DirectionEnum::DIRECTION_ENUM_RESPONSE);
    error_response.set_type("error");
    error_response.set_request_id("");  // No request ID yet
    error_response.set_session_id(session_id);
    error_response.set_participant_id(participant_id);
    auto* error = error_response.mutable_error_response();
    error->set_code("RESOURCE_EXHAUSTED");
    error->set_message("Session limit reached");

    stream->Write(error_response);

    return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Session limit reached");
  }

  // Create VadTurnPipeline for this session
  if (shared_silero_vad_ && shared_smart_turn_detector_) {
    session->vad_turn_pipeline = std::make_unique<VadTurnPipeline>(
        session_id, shared_silero_vad_, shared_smart_turn_detector_, session);
    session->vad_turn_pipeline->Start();
    session->audio_track_sink->SetVadTurnPipeline(session->vad_turn_pipeline.get());
    std::cout << "[Service] Created VadTurnPipeline for session: " << session_id << "\n";
  } else {
    std::cerr << "[Service] WARNING: Shared VAD/Turn models not initialized, "
              << "VAD pipeline not created\n";
  }

  // Create and attach control stream to session
  auto control_stream = std::make_unique<ControlStream>(session_id, session, stream);
  control_stream->Start();
  session->streams->SetControlStream(std::move(control_stream));

  v1::ListeningEngineControlMessage connected_ack;
  connected_ack.set_type("control_stream_connected");
  connected_ack.set_direction(v1::DirectionEnum::DIRECTION_ENUM_RESPONSE);
  connected_ack.set_session_id(session_id);
  connected_ack.set_participant_id(participant_id);
  if (!stream->Write(connected_ack)) {
    std::cerr << "[Service] Failed to write initial connected_ack for session " << session_id
              << "\n";
    return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to write initial ack");
  }
  std::cout << "[Service] Sent control_stream_connected ack for session: " << session_id << "\n";

  while (!context->IsCancelled() && !session->IsRemovalRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  RemoveSession(session_id); // Safe if already removed via close_session

  std::cout << "[Service] Control stream ended for session: " << session_id << "\n";
  return grpc::Status::OK;
}

grpc::Status ListeningEngineServiceImpl::Events(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<v1::EngineToOrchestratorEvent, v1::OrchestratorToEngineEvent>*
        stream) {
  std::cout << "[Service] Events stream established\n";

  // Extract session_id from metadata
  std::string session_id;
  std::string participant_id;
  const auto& metadata = context->client_metadata();

  auto session_it = metadata.find("session-id");
  if (session_it != metadata.end()) {
    session_id = std::string(session_it->second.data(), session_it->second.length());
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "'session-id' metadata is required");
  }

  auto participant_it = metadata.find("participant-id");
  if (participant_it != metadata.end()) {
    participant_id = std::string(participant_it->second.data(), participant_it->second.length());
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "'participant-id' metadata is required");
  }

  std::cout << "[Service] Event stream for session: " << session_id << "\n";

  // Get existing session
  Session* session = nullptr;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
      session = it->second.get();
    }
  }

  if (!session) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "Session not found");
  }

  // Create and attach event stream to session
  auto event_stream = std::make_unique<EventStream>(session_id, session, stream);
  event_stream->Start();
  session->streams->SetEventStream(std::move(event_stream));

  if (session->streams->GetMediaStream()) {
    auto* media_stream_ptr = session->streams->GetMediaStream();
    session->streams->GetEventStream()->RegisterEventListener(
        [media_stream_ptr](const std::string& event_type, uint64_t timestamp_ms) {
          media_stream_ptr->OnVadTurnEvent(event_type, timestamp_ms);
        });
    std::cout << "[Service] Wired MediaStream to EventStream for session: " << session_id << "\n";
  }

  v1::EngineToOrchestratorEvent connected_event;
  connected_event.set_type("events_stream_connected");
  connected_event.set_session_id(session_id);
  connected_event.set_participant_id(participant_id);
  if (!stream->Write(connected_event)) {
    std::cerr << "[Service] Failed to write initial connected_event for session " << session_id
              << "\n";
    return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to write initial event");
  }
  std::cout << "[Service] Sent events_stream_connected event for session: " << session_id << "\n";

  while (!context->IsCancelled() && !session->IsRemovalRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  RemoveSession(session_id); // Safe if already removed via close_session

  std::cout << "[Service] Event stream ended for session: " << session_id << "\n";
  return grpc::Status::OK;
}

grpc::Status ListeningEngineServiceImpl::Media(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<v1::ListeningEngineMediaPayload, v1::ListeningEngineMediaAck>*
        stream) {
  std::cout << "[Service] Media stream established\n";

  // Extract session_id from metadata
  std::string session_id;
  std::string participant_id;
  const auto& metadata = context->client_metadata();

  auto session_it = metadata.find("session-id");
  if (session_it != metadata.end()) {
    session_id = std::string(session_it->second.data(), session_it->second.length());
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "'session-id' metadata is required");
  }

  auto participant_it = metadata.find("participant-id");
  if (participant_it != metadata.end()) {
    participant_id = std::string(participant_it->second.data(), participant_it->second.length());
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "'participant-id' metadata is required");
  }

  std::cout << "[Service] Media stream for session: " << session_id << "\n";

  // Get existing session
  Session* session = nullptr;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
      session = it->second.get();
    }
  }

  if (!session) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "Session not found");
  }

  // Create and attach media stream to session
  auto media_stream = std::make_unique<MediaStream>(session_id, session, stream);
  media_stream->Start();
  session->streams->SetMediaStream(std::move(media_stream));

  if (session->streams->GetEventStream()) {
    auto* media_stream_ptr = session->streams->GetMediaStream();
    session->streams->GetEventStream()->RegisterEventListener(
        [media_stream_ptr](const std::string& event_type, uint64_t timestamp_ms) {
          media_stream_ptr->OnVadTurnEvent(event_type, timestamp_ms);
        });
    std::cout << "[Service] Wired MediaStream to EventStream for session: " << session_id << "\n";
  }

  v1::ListeningEngineMediaPayload connected_payload;
  connected_payload.set_type("media_stream_connected");
  connected_payload.set_session_id(session_id);
  connected_payload.set_participant_id(participant_id);
  if (!stream->Write(connected_payload)) {
    std::cerr << "[Service] Failed to write initial connected_ack for media stream " << session_id
              << "\n";
    return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to write initial media ack");
  }
  std::cout << "[Service] Sent media_stream_connected ack for session: " << session_id << "\n";

  while (!context->IsCancelled() && !session->IsRemovalRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  RemoveSession(session_id); // Safe if already removed via close_session

  std::cout << "[Service] Media stream ended for session: " << session_id << "\n";
  return grpc::Status::OK;
}

}  // namespace saasy::listening_engine
