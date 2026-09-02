#include "event_stream.h"

#include <iostream>

#include "json_proto_converter.h"
#include "session.h"

namespace saasy::listening_engine {

EventStream::EventStream(
    const std::string& session_id, Session* session,
    grpc::ServerReaderWriter<v1::EngineToOrchestratorEvent, v1::OrchestratorToEngineEvent>* stream)
    : session_id_(session_id), session_(session), stream_(stream) {
  std::cout << "[EventStream] Created event stream for session: " << session_id << "\n";
}

EventStream::~EventStream() {
  Stop();

  // It is mandatory to join threads before their std::thread object is destroyed.
  // This waits for the threads to finish their work and exit gracefully,
  // preventing std::terminate from being called and crashing the application.
  if (incoming_event_thread_.joinable()) {
    incoming_event_thread_.join();
  }
  if (outgoing_event_thread_.joinable()) {
    outgoing_event_thread_.join();
  }

  std::cout << "[EventStream] Destroyed event stream for session: " << session_id_ << "\n";
}

void EventStream::Start() {
  running_ = true;
  incoming_event_thread_ = std::thread(&EventStream::ProcessIncomingMessages, this);
  outgoing_event_thread_ = std::thread(&EventStream::ProcessOutgoingEvents, this);
}

void EventStream::Stop() {
  running_ = false;
  queue_cv_.notify_all();
}

void EventStream::SendOnConnectEvent(const std::string& transport_id,
                                     const saasy::shared::v1::DtlsParameters& dtls_params) {
  v1::EngineToOrchestratorEvent event;
  event.set_type("on_connect");
  event.set_session_id(session_id_);
  event.set_participant_id(session_->participant_id);

  auto* on_connect = event.mutable_on_connect();
  on_connect->set_transport_id(transport_id);
  *on_connect->mutable_device_dtls_parameters() = dtls_params;

  QueueEvent(event);
  std::cout << "[EventStream] Queued OnConnect event for transport: " << transport_id << "\n";
}

void EventStream::SendOnSpeechStartedEvent(uint64_t timestamp_ms) {
  v1::EngineToOrchestratorEvent event;
  event.set_type("on_speech_started");
  event.set_session_id(session_id_);
  event.set_participant_id(session_->participant_id);

  auto* on_speech_started = event.mutable_on_speech_started();
  on_speech_started->set_timestamp_ms(timestamp_ms);

  QueueEvent(event);
  NotifyListeners("speech_started", timestamp_ms); 
  std::cout << "[EventStream] Queued OnSpeechStarted event at timestamp: " << timestamp_ms << "\n";
}

void EventStream::SendOnSpeechEndedEvent(uint64_t timestamp_ms) {
  v1::EngineToOrchestratorEvent event;
  event.set_type("on_speech_ended");
  event.set_session_id(session_id_);
  event.set_participant_id(session_->participant_id);

  auto* on_speech_ended = event.mutable_on_speech_ended();
  on_speech_ended->set_timestamp_ms(timestamp_ms);

  QueueEvent(event);
  std::cout << "[EventStream] Queued OnSpeechEnded event at timestamp: " << timestamp_ms << "\n";
}

void EventStream::SendOnUserTurnCompleteEvent(float confidence, uint64_t timestamp_ms) {
  v1::EngineToOrchestratorEvent event;
  event.set_type("on_user_turn_complete");
  event.set_session_id(session_id_);
  event.set_participant_id(session_->participant_id);

  auto* on_user_turn_complete = event.mutable_on_user_turn_complete();
  on_user_turn_complete->set_confidence(confidence);
  on_user_turn_complete->set_timestamp_ms(timestamp_ms);

  QueueEvent(event);
  NotifyListeners("user_turn_complete", timestamp_ms); 
  std::cout << "[EventStream] Queued OnUserTurnComplete event with confidence: " << confidence 
            << " at timestamp: " << timestamp_ms << "\n";
}

void EventStream::QueueEvent(const v1::EngineToOrchestratorEvent& event) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    event_queue_.push(event);
  }
  queue_cv_.notify_one();
}

void EventStream::ProcessIncomingMessages() {
  std::cout << "[EventStream] Starting incoming message processor\n";

  v1::OrchestratorToEngineEvent message;
  while (running_ && stream_->Read(&message)) {
    std::cout << "[EventStream] Received message type: " << message.type()
              << " from orchestrator\n";

    // Currently, OrchestratorToEngineEvent has no defined events
    // This thread is here for future extensibility
    // For now, just log any unexpected messages
    std::cerr << "[EventStream] Received unexpected message on event stream\n";
  }

  std::cout << "[EventStream] Incoming message processor ended\n";
}

void EventStream::ProcessOutgoingEvents() {
  std::cout << "[EventStream] Starting outgoing event processor\n";

  while (running_) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return !event_queue_.empty() || !running_; });

    while (!event_queue_.empty() && running_) {
      auto event = event_queue_.front();
      event_queue_.pop();
      lock.unlock();

      if (!stream_->Write(event)) {
        std::cerr << "[EventStream] Failed to write event for session: " << session_id_ << "\n";
        // TODO: Handle write failure - maybe reconnect?
      }

      lock.lock();
    }
  }

  std::cout << "[EventStream] Outgoing event processor ended\n";
}

// TransportListener implementation

TransportListener::TransportListener(Session* session,
                                                         const std::string& transport_id)
    : session_(session), transport_id_(transport_id) {}

std::future<void> TransportListener::OnConnect(
    [[maybe_unused]] mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) {
  std::cout << "[TransportListener] OnConnect called for transport: " << transport_id_
            << "\n";

  try {
    // Convert JSON DTLS parameters to proto
    auto dtls_proto =
        saasy::common::JsonToProto(dtlsParameters, (saasy::shared::v1::DtlsParameters*)nullptr);

    // Get event stream and send OnConnect event (fire and forget)
    if (auto* event_stream = session_->streams->GetEventStream()) {
      event_stream->SendOnConnectEvent(transport_id_, dtls_proto);
    } else {
      throw std::runtime_error("Event stream not available");
    }

    // Return completed future immediately
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();

  } catch (const std::exception& e) {
    std::cerr << "[TransportListener] Failed to send OnConnect event: " << e.what()
              << "\n";
    std::promise<void> promise;
    promise.set_exception(std::current_exception());
    return promise.get_future();
  }
}

void TransportListener::OnConnectionStateChange(
    [[maybe_unused]] mediasoupclient::Transport* transport, const std::string& connectionState) {
  std::cout << "[TransportListener] Connection state changed to: " << connectionState << "\n";

  // Log connection states
  if (connectionState == "connected") {
    std::cout << "[TransportListener] Transport successfully connected!\n";
  } else if (connectionState == "failed") {
    std::cerr << "[TransportListener] Transport connection failed!\n";
  }
}

void TransportListener::OnTransportClose(mediasoupclient::Consumer* consumer) {
  std::cout << "[TransportListener] Transport closed for consumer: " << consumer->GetId() << "\n";
  // The consumer will be automatically closed by mediasoupclient
}

void EventStream::RegisterEventListener(EventCallback callback) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  event_listeners_.push_back(std::move(callback));
  std::cout << "[EventStream] Registered event listener for session: " << session_id_ << "\n";
}

void EventStream::NotifyListeners(const std::string& event_type, uint64_t timestamp_ms) {
  std::lock_guard<std::mutex> lock(listeners_mutex_);
  for (const auto& callback : event_listeners_) {
    callback(event_type, timestamp_ms);
  }
}

}  // namespace saasy::listening_engine
