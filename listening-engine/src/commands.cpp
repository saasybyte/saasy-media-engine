#include "commands.h"

#include <iostream>
#include <json.hpp>
#include <mediasoupclient/mediasoupclient.hpp>

#include "event_stream.h"
#include "fake_audio_device_module.h"
#include "json_proto_converter.h"
#include "proto_json_converter.h"
#include "session.h"
#include "webrtc_factory.h"

namespace saasy::listening_engine {

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

    std::cout << "[Commands] Creating RecvTransport with TransportListener...\n";
    session_->transport.reset(session_->device->CreateRecvTransport(
        session_->transport_listener.get(), request_.transport_id().id(), ice_params_json,
        ice_candidates_json, dtls_params_json, nlohmann::json(nullptr), &peer_options));

    std::cout << "[Commands] RecvTransport created successfully with event-based callbacks.\n";

    v1::CreateTransportResponse response;
    InvokeCallback(response);
  } catch (const std::exception& e) {
    std::cerr << "[Commands] CreateTransportCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void CreateConsumerCommand::Execute() {
  std::cout << "[Commands] Executing CreateConsumerCommand\n";

  try {
    if (!session_->transport) {
      throw std::runtime_error("Transport not created yet");
    }

    if (request_.kind() != saasy::shared::v1::MEDIA_KIND_AUDIO) {
      throw std::runtime_error("Only audio consumers are supported");
    }

    auto rtp_params_json = saasy::common::ProtoToJson(request_.rtp_parameters());

    std::cout << "[Commands] Creating consumer for consumer_id: "
              << request_.consumer_id().id() << ", producer_id: " << request_.producer_id().id()
              << ", kind: " << request_.kind() << "\n";

    // Note: This Consume() call will trigger TransportListener::OnConnect
    // if this is the first consumer. OnConnect will send the DTLS parameters
    // to the orchestrator via the event stream.
    auto* consumer = session_->transport->Consume(
        session_->transport_listener.get(),  // Use transport listener as consumer listener
        request_.consumer_id().id(),         // consumer id from server
        request_.producer_id().id(),         // producer id
        "audio",                             // kind
        &rtp_params_json);                   // RTP parameters

    if (!consumer) {
      throw std::runtime_error("Failed to create consumer");
    }

    auto* track = consumer->GetTrack();
    if (!track) {
      throw std::runtime_error("Consumer has no track");
    }

    auto* audio_track = static_cast<webrtc::AudioTrackInterface*>(track);
    if (!audio_track) {
      throw std::runtime_error("Failed to cast to audio track");
    }

    audio_track->AddSink(session_->audio_track_sink.get());
    std::cout << "[Commands] Audio sink attached to consumer track\n";

    std::string device_consumer_id = consumer->GetId();
    {
      std::lock_guard<std::mutex> lock(session_->consumers_mutex_);
      session_->consumers[device_consumer_id] = std::unique_ptr<mediasoupclient::Consumer>(consumer);
    }

    v1::CreateConsumerResponse response;
    response.set_device_consumer_id(device_consumer_id);

    std::cout << "[Commands] Consumer created with device ID: " << device_consumer_id << "\n";
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] CreateConsumerCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

void ResumeConsumerCommand::Execute() {
  std::cout << "[Commands] Executing ResumeConsumerCommand\n";

  try {
    auto it = session_->consumers.find(request_.device_consumer_id());
    if (it == session_->consumers.end()) {
      throw std::runtime_error("Consumer not found: " + request_.device_consumer_id());
    }

    auto* consumer = it->second.get();
    if (!consumer) {
      throw std::runtime_error("Consumer is null");
    }

    consumer->Resume();
    std::cout << "[Commands] Consumer resumed: " << request_.device_consumer_id() << "\n";

    v1::ResumeConsumerResponse response;
    InvokeCallback(response);

  } catch (const std::exception& e) {
    std::cerr << "[Commands] ResumeConsumerCommand failed: " << e.what() << "\n";
    InvokeCallback(e.what());
  }
}

}  // namespace saasy::listening_engine
