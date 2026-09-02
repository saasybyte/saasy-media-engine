#include "control_stream.h"

#include <iostream>
#include <sstream>

#include "commands.h"
#include "command_framework.h"
#include "session.h"

namespace saasy::speaking_engine {

ControlStream::ControlStream(const std::string& session_id, Session* session,
                             grpc::ServerReaderWriter<v1::SpeakingEngineControlMessage,
                                                      v1::SpeakingEngineControlMessage>* stream)
    : session_id_(session_id), session_(session), stream_(stream) {
  std::cout << "[ControlStream] Created control stream for session: " << session_id << "\n";
}

ControlStream::~ControlStream() {
  Stop();

  // It is mandatory to join threads before their std::thread object is destroyed.
  // This waits for the threads to finish their work and exit gracefully,
  // preventing std::terminate from being called and crashing the application.
  if (incoming_control_thread_.joinable()) {
    incoming_control_thread_.join();
  }
  if (outgoing_control_thread_.joinable()) {
    outgoing_control_thread_.join();
  }
  std::cout << "[ControlStream] Destroyed control stream for session: " << session_id_ << "\n";
}

void ControlStream::Start() {
  running_ = true;
  incoming_control_thread_ = std::thread(&ControlStream::ProcessIncomingMessages, this);
  outgoing_control_thread_ = std::thread(&ControlStream::ProcessOutgoingMessages, this);
}

void ControlStream::Stop() {
  if (running_.exchange(false)) {
    queue_cv_.notify_all();
  }
}

void ControlStream::QueueMessage(const v1::SpeakingEngineControlMessage& message) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    outgoing_queue_.push(message);
  }
  queue_cv_.notify_one();
}

void ControlStream::ProcessIncomingMessages() {
  std::cout << "[ControlStream] Starting incoming message processor for session: " << session_id_
            << "\n";

  v1::SpeakingEngineControlMessage message;
  while (running_ && stream_->Read(&message)) {
    if (message.direction() == v1::DirectionEnum::DIRECTION_ENUM_RESPONSE) {
      std::lock_guard<std::mutex> lock(pending_requests_mutex_);
      auto it = pending_requests_.find(message.request_id());
      if (it != pending_requests_.end()) {
        // Invoke the callback with the response
        it->second(message);
        pending_requests_.erase(it);
      } else {
        std::cerr << "[ControlStream] Received response for unknown request_id: "
                  << message.request_id() << "\n";
      }
    } else {
      HandleIncomingRequest(message);
    }
  }

  std::cout << "[ControlStream] Incoming message processor ended for session: " << session_id_
            << "\n";
  Stop();
}

void ControlStream::ProcessOutgoingMessages() {
  std::cout << "[ControlStream] Starting outgoing message processor for session: " << session_id_
            << "\n";

  while (running_) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return !outgoing_queue_.empty() || !running_; });

    if (!running_) {
      break;
    }

    while (!outgoing_queue_.empty()) {
      v1::SpeakingEngineControlMessage message = outgoing_queue_.front();
      outgoing_queue_.pop();
      lock.unlock();

      if (!stream_->Write(message)) {
        std::cerr << "[ControlStream] Failed to write message for session: " << session_id_
                  << "\n";
      }

      lock.lock();
    }
  }

  std::cout << "[ControlStream] Outgoing message processor ended for session: " << session_id_
            << "\n";
}

void ControlStream::HandleIncomingRequest(const v1::SpeakingEngineControlMessage& request) {
  std::cout << "[ControlStream] Handling control request type: " << request.type()
            << " with request_id: " << request.request_id() << "\n";

  if (request.session_id() != session_id_) {
    SendErrorResponse(request.request_id(), request.session_id(), request.participant_id(),
                      "INVALID_ARGUMENT", "Session ID mismatch");
    return;
  }

  try {
    // Create a lambda that captures request metadata and will send response when command completes
    auto create_response_callback = [this, request]([[maybe_unused]] const auto& cmd_response, 
                                                    std::optional<std::string> error) {
      v1::SpeakingEngineControlMessage response;
      response.set_direction(v1::DirectionEnum::DIRECTION_ENUM_RESPONSE);
      response.set_request_id(request.request_id());
      response.set_session_id(session_id_);
      response.set_participant_id(request.participant_id());
      
      if (error.has_value()) {
        response.set_type("error");
        auto* error_resp = response.mutable_error_response();
        error_resp->set_code("INTERNAL");
        error_resp->set_message(error.value());
      } else {
        response.set_type(request.type());
        // The specific response will be set by the individual handlers below
      }
      
      return response;
    };

    if (request.has_load_device_request()) {
      auto callback = [this, create_response_callback](const v1::LoadDeviceResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_load_device_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<LoadDeviceCommand>(request.load_device_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::LoadDeviceResponse>>(std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_get_device_rtp_capabilities_request()) {
      auto callback = [this, create_response_callback](const v1::GetDeviceRtpCapabilitiesResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_get_device_rtp_capabilities_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<GetDeviceRtpCapabilitiesCommand>(
          request.get_device_rtp_capabilities_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::GetDeviceRtpCapabilitiesResponse>>(
          std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_create_transport_request()) {
      auto callback = [this, create_response_callback](const v1::CreateTransportResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_create_transport_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<CreateTransportCommand>(request.create_transport_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::CreateTransportResponse>>(std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_start_production_request()) {
      auto callback = [this, create_response_callback](const v1::StartProductionResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_start_production_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<StartProductionCommand>(request.start_production_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::StartProductionResponse>>(std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_flush_audio_request()) {
      auto callback = [this, create_response_callback](const v1::FlushAudioResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_flush_audio_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<FlushAudioCommand>(request.flush_audio_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::FlushAudioResponse>>(std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_speech_generation_complete_request()) {
      auto callback = [this, create_response_callback](const v1::SpeechGenerationCompleteResponse& cmd_response,
                                                      std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_speech_generation_complete_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<SpeechGenerationCompleteCommand>(
        request.speech_generation_complete_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::SpeechGenerationCompleteResponse>>(
        std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));

    } else if (request.has_close_session_request()) {
      v1::SpeakingEngineControlMessage response;
      response.set_direction(v1::DirectionEnum::DIRECTION_ENUM_RESPONSE);
      response.set_request_id(request.request_id());
      response.set_session_id(session_id_);
      response.set_participant_id(request.participant_id());
      response.set_type("close_session");
      *response.mutable_close_session_response() = v1::CloseSessionResponse{};
      QueueMessage(response);
      
      // Trigger full session removal (stops all streams, cleans up resources)
      session_->RequestRemoval();
    } else {
      SendErrorResponse(request.request_id(), request.session_id(), request.participant_id(),
                        "UNIMPLEMENTED", "Unknown request type: " + request.type());
    }
  } catch (const std::exception& e) {
    std::cerr << "[ControlStream] Command creation failed: " << e.what() << "\n";
    SendErrorResponse(request.request_id(), request.session_id(), request.participant_id(),
                      "INTERNAL", e.what());
  }
}

void ControlStream::SendErrorResponse(const std::string& request_id, const std::string& session_id,
                                      const std::string& participant_id,
                                      const std::string& error_code,
                                      const std::string& error_message) {
  v1::SpeakingEngineControlMessage response;
  response.set_direction(v1::DirectionEnum::DIRECTION_ENUM_RESPONSE);
  response.set_type("error");
  response.set_request_id(request_id);
  response.set_session_id(session_id);
  response.set_participant_id(participant_id);

  auto* error = response.mutable_error_response();
  error->set_code(error_code);
  error->set_message(error_message);

  QueueMessage(response);
}

void ControlStream::GetRouterProducerId(
    const std::string& transport_id, saasy::shared::v1::MediaKind kind,
    const saasy::shared::v1::RtpParameters& rtp_parameters,
    std::function<void(const std::string&, std::optional<std::string>)> callback) {
  
  v1::SpeakingEngineControlMessage request;
  request.set_direction(v1::DirectionEnum::DIRECTION_ENUM_REQUEST);
  request.set_type("get_router_producer_id");
  request.set_request_id(GenerateRequestId());
  request.set_session_id(session_id_);
  request.set_participant_id(session_->participant_id);

  auto* get_producer_id_req = request.mutable_get_router_producer_id_request();
  get_producer_id_req->set_transport_id(transport_id);
  get_producer_id_req->set_kind(kind);
  *get_producer_id_req->mutable_rtp_parameters() = rtp_parameters;

  // Store callback for when response arrives
  {
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    pending_requests_[request.request_id()] = [callback](const v1::SpeakingEngineControlMessage& response) {
      if (response.has_get_router_producer_id_response()) {
        callback(response.get_router_producer_id_response().producer_id(), std::nullopt);
      } else if (response.has_error_response()) {
        auto& err = response.error_response();
        callback("", "Orchestrator error: " + err.code() + " - " + err.message());
      } else {
        callback("", "Invalid response type");
      }
    };
  }

  QueueMessage(request);
}

std::string ControlStream::GenerateRequestId() {
  std::stringstream ss;
  ss << "engine_" << session_id_ << "_" << request_counter_.fetch_add(1);
  return ss.str();
}

}  // namespace saasy::speaking_engine
