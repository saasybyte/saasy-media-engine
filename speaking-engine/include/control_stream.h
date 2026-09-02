#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "event_stream.h"
#include "media_stream.h"
#include "protos/speaking_engine/v1/speaking_engine.grpc.pb.h"  // IWYU pragma: keep

namespace saasy::speaking_engine {

// Forward declarations
struct Session;

class SpeakingEngineServiceImpl;

class ControlStream {
 public:
  ControlStream(const std::string& session_id, Session* session,
                grpc::ServerReaderWriter<v1::SpeakingEngineControlMessage,
                                         v1::SpeakingEngineControlMessage>* stream);

  ~ControlStream();

  void Start();

  void Stop();

  void QueueMessage(const v1::SpeakingEngineControlMessage& message);

  void GetRouterProducerId(
      const std::string& transport_id, saasy::shared::v1::MediaKind kind,
      const saasy::shared::v1::RtpParameters& rtp_parameters,
      std::function<void(const std::string&, std::optional<std::string>)> callback);

 private:
  void ProcessIncomingMessages();

  void ProcessOutgoingMessages();

  void HandleIncomingRequest(const v1::SpeakingEngineControlMessage& request);

  void SendErrorResponse(const std::string& request_id, const std::string& session_id,
                         const std::string& participant_id, const std::string& error_code,
                         const std::string& error_message);

  std::string GenerateRequestId();

  using ResponseCallback = std::function<void(const v1::SpeakingEngineControlMessage&)>;
  std::string session_id_;
  Session* session_;
  grpc::ServerReaderWriter<v1::SpeakingEngineControlMessage, v1::SpeakingEngineControlMessage>*
      stream_;
  std::atomic<bool> running_{false};
  std::thread incoming_control_thread_;
  std::thread outgoing_control_thread_;
  std::queue<v1::SpeakingEngineControlMessage> outgoing_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::atomic<uint64_t> request_counter_{0};
  std::mutex pending_requests_mutex_;
  std::unordered_map<std::string, ResponseCallback> pending_requests_;
};

}  // namespace saasy::speaking_engine
