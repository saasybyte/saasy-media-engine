#pragma once

#include <map>
#include <mediasoupclient/Device.hpp>
#include <mediasoupclient/Transport.hpp>
#include <memory>
#include <mutex>

#include "protos/speaking_engine/v1/speaking_engine.grpc.pb.h"
#include "session.h"

namespace saasy::speaking_engine {

class SpeakingEngineServiceImpl final : public v1::SpeakingEngineService::Service {
 public:
  SpeakingEngineServiceImpl();

  ~SpeakingEngineServiceImpl();

  grpc::Status HealthCheck(grpc::ServerContext* context, const v1::HealthCheckRequest* request,
                           v1::HealthCheckResponse* response) override;

  grpc::Status Control(grpc::ServerContext* context,
                       grpc::ServerReaderWriter<v1::SpeakingEngineControlMessage,
                                                v1::SpeakingEngineControlMessage>* stream) override;

  grpc::Status Events(grpc::ServerContext* context,
                      grpc::ServerReaderWriter<v1::EngineToOrchestratorEvent,
                                               v1::OrchestratorToEngineEvent>* stream) override;

  grpc::Status Media(grpc::ServerContext* context,
                     grpc::ServerReaderWriter<v1::SpeakingEngineMediaAck,
                                              v1::SpeakingEngineMediaPayload>* stream) override;

 private:
  static constexpr size_t kMaxSessions = 20;  // TODO: env var
  std::map<std::string, std::unique_ptr<Session>> sessions_;
  mutable std::mutex sessions_mutex_;

  Session* GetOrCreateSession(const std::string& session_id, const std::string& participant_id);

  void RemoveSession(const std::string& session_id);
};

}  // namespace saasy::speaking_engine
