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
#include "protos/listening_engine/v1/listening_engine.grpc.pb.h"  // IWYU pragma: keep

namespace saasy::listening_engine {

// Forward declarations
struct Session;

class ListeningEngineServiceImpl;

class ControlStream {
 public:
  ControlStream(const std::string& session_id, Session* session,
                grpc::ServerReaderWriter<v1::ListeningEngineControlMessage,
                                         v1::ListeningEngineControlMessage>* stream);

  ~ControlStream();

  void Start();

  void Stop();

  void QueueMessage(const v1::ListeningEngineControlMessage& message);

 private:
  void ProcessIncomingMessages();

  void ProcessOutgoingMessages();

  void HandleIncomingRequest(const v1::ListeningEngineControlMessage& request);

  void SendErrorResponse(const std::string& request_id, const std::string& session_id,
                         const std::string& participant_id, const std::string& error_code,
                         const std::string& error_message);

  using ResponseCallback = std::function<void(const v1::ListeningEngineControlMessage&)>;
  std::string session_id_;
  Session* session_;
  grpc::ServerReaderWriter<v1::ListeningEngineControlMessage, v1::ListeningEngineControlMessage>*
      stream_;
  std::atomic<bool> running_{false};
  std::thread incoming_control_thread_;
  std::thread outgoing_control_thread_;
  std::queue<v1::ListeningEngineControlMessage> outgoing_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::atomic<uint64_t> request_counter_{0};
  std::mutex pending_requests_mutex_;
  std::unordered_map<std::string, ResponseCallback> pending_requests_;
};

}  // namespace saasy::listening_engine
