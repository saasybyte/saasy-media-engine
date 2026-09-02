#pragma once

#include "command_framework.h"
#include "protos/listening_engine/v1/listening_engine.pb.h"

namespace saasy::listening_engine {

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

class CreateConsumerCommand : public common::CommandWithResponse<v1::CreateConsumerResponse> {
 public:
  CreateConsumerCommand(const v1::CreateConsumerRequest& request, Session* session,
                        common::CommandCallback<v1::CreateConsumerResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::CreateConsumerRequest request_;
  Session* session_;
};

class ResumeConsumerCommand : public common::CommandWithResponse<v1::ResumeConsumerResponse> {
 public:
  ResumeConsumerCommand(const v1::ResumeConsumerRequest& request, Session* session,
                        common::CommandCallback<v1::ResumeConsumerResponse> callback)
      : CommandWithResponse(std::move(callback)), request_(request), session_(session) {}

  void Execute() override;

 private:
  v1::ResumeConsumerRequest request_;
  Session* session_;
};

}  // namespace saasy::listening_engine
