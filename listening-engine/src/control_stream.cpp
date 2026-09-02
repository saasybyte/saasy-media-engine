#include "control_stream.h"

#include <iostream>

#include "commands.h"
#include "command_framework.h"
#include "session.h"

namespace saasy::listening_engine {

ControlStream::ControlStream(const std::string& session_id, Session* session,
                             grpc::ServerReaderWriter<v1::ListeningEngineControlMessage,
                                                      v1::ListeningEngineControlMessage>* stream)
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

void ControlStream::QueueMessage(const v1::ListeningEngineControlMessage& message) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    outgoing_queue_.push(message);
  }
  queue_cv_.notify_one();
}

void ControlStream::ProcessIncomingMessages() {
  std::cout << "[ControlStream] Starting incoming message processor for session: " << session_id_
            << "\n";

  v1::ListeningEngineControlMessage message;
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
      v1::ListeningEngineControlMessage message = outgoing_queue_.front();
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

void ControlStream::HandleIncomingRequest(const v1::ListeningEngineControlMessage& request) {
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
      v1::ListeningEngineControlMessage response;
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
      
    } else if (request.has_create_consumer_request()) {
      auto callback = [this, create_response_callback](const v1::CreateConsumerResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_create_consumer_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<CreateConsumerCommand>(request.create_consumer_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::CreateConsumerResponse>>(std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_resume_consumer_request()) {
      auto callback = [this, create_response_callback](const v1::ResumeConsumerResponse& cmd_response,
                                                       std::optional<std::string> error) {
        auto response = create_response_callback(cmd_response, error);
        if (!error.has_value()) {
          *response.mutable_resume_consumer_response() = cmd_response;
        }
        QueueMessage(response);
      };
      
      auto cmd = std::make_unique<ResumeConsumerCommand>(request.resume_consumer_request(), session_, callback);
      auto wrapper = std::make_unique<common::ResponseCommandWrapper<v1::ResumeConsumerResponse>>(std::move(cmd));
      session_->command_processor->GetCommandQueue()->Push(std::move(wrapper));
      
    } else if (request.has_close_session_request()) {
      v1::ListeningEngineControlMessage response;
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
  v1::ListeningEngineControlMessage response;
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

}  // namespace saasy::listening_engine
