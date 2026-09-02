#pragma once

#include <map>
#include <mediasoupclient/Consumer.hpp>
#include <mediasoupclient/Device.hpp>
#include <mediasoupclient/Transport.hpp>
#include <memory>
#include <mutex>

#include "protos/listening_engine/v1/listening_engine.grpc.pb.h"
#include "session.h"
#include "vad_turn/silero_vad.h"
#include "vad_turn/smart_turn_detector.h"

namespace saasy::listening_engine {

struct ListeningEngineConfig {
  std::string mel_filters_path = "listening-engine/assets/mel_80.bin";
  std::string vad_model_path = "listening-engine/models/silero_vad_v6.onnx";
  std::string turn_model_path = "listening-engine/models/smart_turn_v3.onnx";
  VadConfig vad_config;
  TurnDetectorConfig turn_config;
};

class ListeningEngineServiceImpl final : public v1::ListeningEngineService::Service {
 public:
  explicit ListeningEngineServiceImpl(
      const ListeningEngineConfig& config = ListeningEngineConfig());

  ~ListeningEngineServiceImpl();

  grpc::Status HealthCheck(grpc::ServerContext* context, const v1::HealthCheckRequest* request,
                           v1::HealthCheckResponse* response) override;

  grpc::Status Control(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<v1::ListeningEngineControlMessage,
                               v1::ListeningEngineControlMessage>* stream) override;

  grpc::Status Events(grpc::ServerContext* context,
                      grpc::ServerReaderWriter<v1::EngineToOrchestratorEvent,
                                               v1::OrchestratorToEngineEvent>* stream) override;

  grpc::Status Media(grpc::ServerContext* context,
                     grpc::ServerReaderWriter<v1::ListeningEngineMediaPayload,
                                              v1::ListeningEngineMediaAck>* stream) override;

 private:
  static constexpr size_t kMaxSessions = 20;  // TODO: env var
  ListeningEngineConfig config_;
  std::map<std::string, std::unique_ptr<Session>> sessions_;
  mutable std::mutex sessions_mutex_;
  std::shared_ptr<SileroVAD> shared_silero_vad_;
  std::shared_ptr<SmartTurnDetector> shared_smart_turn_detector_;

  void Initialize();

  Session* GetOrCreateSession(const std::string& session_id, const std::string& participant_id);

  void RemoveSession(const std::string& session_id);
};

}  // namespace saasy::listening_engine
