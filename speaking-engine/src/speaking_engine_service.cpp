#include "speaking_engine_service.h"

#include <iostream>

#include "event_stream.h"
#include "media_stream.h"

namespace saasy::speaking_engine {

SpeakingEngineServiceImpl::SpeakingEngineServiceImpl() {
  std::cout << "[Service] Speaking Engine Service initialized (kMaxSessions=" << kMaxSessions
            << ")\n";
}

SpeakingEngineServiceImpl::~SpeakingEngineServiceImpl() {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  sessions_.clear();
}

Session* SpeakingEngineServiceImpl::GetOrCreateSession(const std::string& session_id,
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

void SpeakingEngineServiceImpl::RemoveSession(const std::string& session_id) {
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

grpc::Status SpeakingEngineServiceImpl::HealthCheck(grpc::ServerContext* /*context*/,
                                                    const v1::HealthCheckRequest* /*request*/,
                                                    v1::HealthCheckResponse* response) {
  response->set_alive(true);
  return grpc::Status::OK;
}

grpc::Status SpeakingEngineServiceImpl::Control(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<v1::SpeakingEngineControlMessage, v1::SpeakingEngineControlMessage>*
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
    v1::SpeakingEngineControlMessage error_response;
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

  // Create and attach control stream to session
  auto control_stream = std::make_unique<ControlStream>(session_id, session, stream);
  control_stream->Start();
  session->streams->SetControlStream(std::move(control_stream));

  v1::SpeakingEngineControlMessage connected_ack;
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

grpc::Status SpeakingEngineServiceImpl::Events(
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

  v1::EngineToOrchestratorEvent connected_event;
  connected_event.set_type("event_stream_connected");
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

grpc::Status SpeakingEngineServiceImpl::Media(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<v1::SpeakingEngineMediaAck, v1::SpeakingEngineMediaPayload>* stream) {
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

  v1::SpeakingEngineMediaAck connected_ack;
  connected_ack.set_type("media_stream_connected");
  connected_ack.set_session_id(session_id);
  connected_ack.set_participant_id(participant_id);
  if (!stream->Write(connected_ack)) {
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

}  // namespace saasy::speaking_engine
