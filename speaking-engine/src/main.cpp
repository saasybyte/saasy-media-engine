#include <grpcpp/grpcpp.h>
#include <signal.h>

#include <iostream>
#include <mediasoupclient.hpp>
#include <memory>
#include <string>

#include "speaking_engine_service.h"
#include "webrtc_factory.h"

std::unique_ptr<grpc::Server> server;

void SignalHandler([[maybe_unused]] int signal) {
  std::cout << "\n[Main] Shutting down gracefully...\n";
  if (server) {
    server->Shutdown();
  }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  std::cout << "[Main] Initializing WebRTC components...\n";
  mediasoupclient::Initialize();
  saasy::common::WebRTCFactory::Initialize();

  saasy::speaking_engine::SpeakingEngineServiceImpl service;

  std::string uds_address = "unix:///tmp/speaking-engine.sock";
  grpc::EnableDefaultHealthCheckService(true);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(uds_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  server = builder.BuildAndStart();

  if (!server) {
    std::cerr << "[Main] Failed to start gRPC server\n";
    return 1;
  }

  std::cout << "[Main] Speaking Engine listening on " << uds_address << "\n";
  std::cout << "[Main] Multi-session mode enabled\n";
  std::cout << "[Main] Press Ctrl+C to stop\n";

  // Wait for shutdown
  server->Wait();
  std::cout << "[Main] Shutdown complete\n";

  saasy::common::WebRTCFactory::Shutdown();
  mediasoupclient::Cleanup();

  return 0;
}
