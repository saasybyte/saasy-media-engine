#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <json.hpp>
#include <mediasoupclient/Transport.hpp>
#include <mutex>
#include <queue>
#include <thread>
#include <string>

#include "protos/speaking_engine/v1/speaking_engine.grpc.pb.h"  // IWYU pragma: keep

namespace saasy::speaking_engine {

// Forward declarations
struct Session;

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

  void SendOnPlaybackCompleteEvent();

  void QueueEvent(const v1::EngineToOrchestratorEvent& event);

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
};

class TransportListener : public mediasoupclient::SendTransport::Listener {
 public:
  TransportListener(Session* session, const std::string& transport_id);

  ~TransportListener() = default;

  std::future<void> OnConnect(mediasoupclient::Transport* transport,
                              const nlohmann::json& dtlsParameters) override;

  std::future<std::string> OnProduce(mediasoupclient::SendTransport* transport,
                                     const std::string& kind, nlohmann::json rtpParameters,
                                     const nlohmann::json& appData) override;

  void OnConnectionStateChange(mediasoupclient::Transport* transport,
                               const std::string& connectionState) override;

  std::future<std::string> OnProduceData(mediasoupclient::SendTransport* transport,
                                         const nlohmann::json& sctpStreamParameters,
                                         const std::string& label, const std::string& protocol,
                                         const nlohmann::json& appData) override;

 private:
  Session* session_;
  std::string transport_id_;
};

class ProducerListener : public mediasoupclient::Producer::Listener {
 public:
  ProducerListener(Session* session);

  ~ProducerListener() = default;

  void OnTransportClose(mediasoupclient::Producer* producer) override;

 private:
  Session* session_;
  std::string producer_id_;
};

}  // namespace saasy::speaking_engine
