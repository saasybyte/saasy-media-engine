#pragma once

#include "command_framework.h"
#include "protos/speaking_engine/v1/speaking_engine.pb.h"

namespace saasy::speaking_engine {

// Forward declaration
struct Session;

class LoadDeviceCommand : public common::CommandWithResponse<v1::LoadDeviceResponse> {
 public:
  LoadDeviceCommand(const v1::LoadDeviceRequest& request, Session* session,
                    common::CommandCallback<v1::LoadDeviceResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::LoadDeviceRequest request_;
  Session* session_;
};

class GetDeviceRtpCapabilitiesCommand
    : public common::CommandWithResponse<v1::GetDeviceRtpCapabilitiesResponse> {
 public:
  GetDeviceRtpCapabilitiesCommand(const v1::GetDeviceRtpCapabilitiesRequest& request,
                                  Session* session,
                                  common::CommandCallback<v1::GetDeviceRtpCapabilitiesResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::GetDeviceRtpCapabilitiesRequest request_;
  Session* session_;
};

class CreateTransportCommand : public common::CommandWithResponse<v1::CreateTransportResponse> {
 public:
  CreateTransportCommand(const v1::CreateTransportRequest& request, Session* session,
                         common::CommandCallback<v1::CreateTransportResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::CreateTransportRequest request_;
  Session* session_;
};

class StartProductionCommand : public common::CommandWithResponse<v1::StartProductionResponse> {
 public:
  StartProductionCommand(const v1::StartProductionRequest& request, Session* session,
                         common::CommandCallback<v1::StartProductionResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::StartProductionRequest request_;
  Session* session_;
};

class FlushAudioCommand : public common::CommandWithResponse<v1::FlushAudioResponse> {
 public:
  FlushAudioCommand(const v1::FlushAudioRequest& request, Session* session,
                    common::CommandCallback<v1::FlushAudioResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::FlushAudioRequest request_;
  Session* session_;
};

class SpeechGenerationCompleteCommand : public common::CommandWithResponse<v1::SpeechGenerationCompleteResponse> {
 public:
  SpeechGenerationCompleteCommand(const v1::SpeechGenerationCompleteRequest& request, Session* session,
                                  common::CommandCallback<v1::SpeechGenerationCompleteResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::SpeechGenerationCompleteRequest request_;
  Session* session_;
};

}  // namespace saasy::speaking_engine
