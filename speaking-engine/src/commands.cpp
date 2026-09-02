#include "commands.h"

#include <iostream>
#include <json.hpp>
#include <mediasoupclient/mediasoupclient.hpp>

#include "event_stream.h"
#include "fake_audio_device_module.h"
#include "json_proto_converter.h"
#include "proto_json_converter.h"
#include "session.h"

namespace saasy::speaking_engine {

void LoadDeviceCommand::Execute() {
  std::cout << "[Commands] Executing LoadDeviceCommand\n";

  try {
    if (session_->device) {
      std::cout << "[Commands] Device already loaded, skipping\n";
      v1::LoadDeviceResponse response;
      InvokeCallback(response);
      return;
    }

    std::cout << "[Commands] Creating mediasoupclient Device\n";
    session_->device = std::make_unique<mediasoupclient::Device>();

    auto router_rtp_capabilities_json =
        saasy::common::ProtoToJson(request_.router_rtp_capabilities());

    // BATMAN fix this
    // Quick fix: Add kind field based on mimeType
    if (router_rtp_capabilities_json.contains("codecs")) {
      auto& codecs = router_rtp_capabilities_json["codecs"];
      for (auto& codec : codecs) {
        std::string mimeType = codec["mimeType"];
        if (mimeType.find("audio/") == 0) {
          codec["kind"] = "audio";
        } else if (mimeType.find("video/") == 0) {
          codec["kind"] = "video";
        }
      }
    }

    std::cout << "[Commands] Loading Device with router capabilities\n";

    // Throwaway ADM for Device::Load() — headless-safe, destroyed with temporary factory
    auto throwaway_adm = webrtc::make_ref_counted<saasy::common::FakeAudioDeviceModule>();
    mediasoupclient::PeerConnection::Options peer_options;
    peer_options.adm = throwaway_adm;

    session_->device->Load(router_rtp_capabilities_json, &peer_options);

    std::cout << "[Commands] Device loaded successfully\n";

    v1::LoadDeviceResponse response;
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] LoadDeviceCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void GetDeviceRtpCapabilitiesCommand::Execute() {
  std::cout << "[Commands] Executing GetDeviceRtpCapabilitiesCommand\n";

  try {
    if (!session_->device) {
      throw std::runtime_error("Device not loaded yet");
    }

    auto device_rtp_capabilities_json = session_->device->GetRtpCapabilities();

    v1::GetDeviceRtpCapabilitiesResponse response;
    *response.mutable_device_rtp_capabilities() = saasy::common::JsonToProto(
        device_rtp_capabilities_json, (saasy::shared::v1::RtpCapabilities*)nullptr);

    std::cout << "[Commands] Got device RTP capabilities\n";
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] GetDeviceRtpCapabilitiesCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void CreateTransportCommand::Execute() {
  std::cout << "[Commands] Executing CreateTransportCommand\n";

  try {
    if (!session_->device) {
      throw std::runtime_error("Device not loaded yet");
    }

    // Create TransportListener that will send events
    session_->transport_listener =
        std::make_unique<TransportListener>(session_, request_.transport_id().id());

    auto ice_params_json = saasy::common::ProtoToJson(request_.ice_parameters());
    auto dtls_params_json = saasy::common::ProtoToJson(request_.dtls_parameters());

    nlohmann::json ice_candidates_json = nlohmann::json::array();
    for (const auto& candidate : request_.ice_candidates()) {
      ice_candidates_json.push_back(saasy::common::ProtoToJson(candidate));
    }

    // Use the session's peer connection factory (which has the custom ADM)
    mediasoupclient::PeerConnection::Options peer_options;
    peer_options.factory = session_->peer_connection_factory.get();

    std::cout << "[Commands] Creating SendTransport with TransportListener...\n";
    session_->transport.reset(session_->device->CreateSendTransport(
        session_->transport_listener.get(), request_.transport_id().id(), ice_params_json,
        ice_candidates_json, dtls_params_json, nlohmann::json(nullptr), &peer_options));

    std::cout << "[Commands] SendTransport created successfully with event-based callbacks.\n";

    v1::CreateTransportResponse response;
    InvokeCallback(response);
  } catch (const std::exception& e) {
    std::cerr << "[Commands] CreateTransportCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void StartProductionCommand::Execute() {
  std::cout << "[Commands] Starting audio production...\n";

  try {
    if (!session_->transport) {
      throw std::runtime_error("Transport not created");
    }
    if (!session_->audio_track) {
      throw std::runtime_error("No audio track available");
    }

    // Create producer listener
    session_->producer_listener = std::make_unique<ProducerListener>(session_);

    nlohmann::json codecOptions = nlohmann::json::parse(R"({"opusStereo": false})");

    // Now we can call Produce() directly - no thread needed!
    // The OnProduce callback will make a synchronous RPC to get the producer ID
    auto* producer = session_->transport->Produce(
        session_->producer_listener.get(), session_->audio_track.get(), nullptr, &codecOptions,  nullptr);

    if (!producer) {
      throw std::runtime_error("Failed to create audio producer");
    }

    std::string device_producer_id = producer->GetId();
    std::cout << "[Commands] Audio producer created with ID: " << device_producer_id << "\n";
    session_->audio_producer_id = device_producer_id;

    // Return the device producer ID in the response
    v1::StartProductionResponse response;
    response.set_device_producer_id(device_producer_id);
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] StartProductionCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void FlushAudioCommand::Execute() {
  std::cout << "[Commands] Executing FlushAudioCommand\n";

  try {
    if (!session_->adm) {
      throw std::runtime_error("Audio device module not initialized");
    }

    // Call the flush method on CustomAudioDeviceModule
    session_->adm->FlushAudioBuffer(request_.fade_duration_ms());

    std::cout << "[Commands] Audio buffer flushed successfully\n";

    v1::FlushAudioResponse response;
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] FlushAudioCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void SpeechGenerationCompleteCommand::Execute() {
  std::cout << "[Commands] Executing SpeechGenerationCompleteCommand\n";

  try {
    if (!session_->adm) {
      throw std::runtime_error("Audio device module not initialized");
    }

    // Mark that all audio chunks have been received from TTS
    session_->adm->SpeechGenerationComplete();
    std::cout << "[Commands] Marked speech generation as complete\n";

    v1::SpeechGenerationCompleteResponse response;
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] SpeechGenerationCompleteCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

}  // namespace saasy::speaking_engine
