#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mediasoupclient/Consumer.hpp>
#include <mediasoupclient/Transport.hpp>
#include <mutex>
#include <queue>
#include <thread>

#include "protos/listening_engine/v1/listening_engine.grpc.pb.h"  // IWYU pragma: keep

namespace saasy::listening_engine {

// Forward declarations
struct Session;
class TransportListener;

using EventCallback = std::function<void(const std::string& event_type, uint64_t timestamp_ms)>;

class EventStream {
 public:
  EventStream(const std::string& session_id, Session* session,
              grpc::ServerReaderWriter<v1::EngineToOrchestratorEvent,
                                       v1::OrchestratorToEngineEvent>* stream);

  ~EventStream();

  void Start();

  void Stop();

  void SendOnConnectEvent(const std::string& transport_id,
                          const saasy::shared::v1::DtlsParameters& dtls_params);

  void SendOnSpeechStartedEvent(uint64_t timestamp_ms);

  void SendOnSpeechEndedEvent(uint64_t timestamp_ms);

  void SendOnUserTurnCompleteEvent(float confidence, uint64_t timestamp_ms);

  void QueueEvent(const v1::EngineToOrchestratorEvent& event);

  void RegisterEventListener(EventCallback callback);

 private:
  void ProcessIncomingMessages();

  void ProcessOutgoingEvents();

  std::string session_id_;
  Session* session_;
  grpc::ServerReaderWriter<v1::EngineToOrchestratorEvent, v1::OrchestratorToEngineEvent>* stream_;
  std::atomic<bool> running_{false};
  std::thread incoming_event_thread_;
  std::thread outgoing_event_thread_;
  std::queue<v1::EngineToOrchestratorEvent> event_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::vector<EventCallback> event_listeners_;
  std::mutex listeners_mutex_;
  
  void NotifyListeners(const std::string& event_type, uint64_t timestamp_ms);
};

class TransportListener : public mediasoupclient::RecvTransport::Listener,
                          public mediasoupclient::Consumer::Listener {
 public:
  TransportListener(Session* session, const std::string& transport_id);

  ~TransportListener() = default;

  std::future<void> OnConnect(mediasoupclient::Transport* transport,
                              const nlohmann::json& dtlsParameters) override;

  void OnConnectionStateChange(mediasoupclient::Transport* transport,
                               const std::string& connectionState) override;

  void OnTransportClose(mediasoupclient::Consumer* consumer) override;

 private:
  Session* session_;
  std::string transport_id_;
};

}  // namespace saasy::listening_engine
