#include "json_proto_converter.h"

#include <algorithm>
#include <cstdint>

namespace saasy::common {

saasy::shared::v1::DtlsRole StringToDtlsRole(const std::string& role) {
  if (role == "auto") {
    return saasy::shared::v1::DTLS_ROLE_AUTO;
  } else if (role == "client") {
    return saasy::shared::v1::DTLS_ROLE_CLIENT;
  } else if (role == "server") {
    return saasy::shared::v1::DTLS_ROLE_SERVER;
  }
  return saasy::shared::v1::DTLS_ROLE_AUTO;
}

saasy::shared::v1::DtlsFingerprintAlgorithm StringToDtlsFingerprintAlgorithm(
    const std::string& algorithm) {
  if (algorithm == "sha-1") {
    return saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA1;
  } else if (algorithm == "sha-224") {
    return saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA224;
  } else if (algorithm == "sha-256") {
    return saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA256;
  } else if (algorithm == "sha-384") {
    return saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA384;
  } else if (algorithm == "sha-512") {
    return saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA512;
  }
  return saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA256;
}

std::string HexStringToBytes(const std::string& hex) {
  std::string bytes;
  std::string hex_clean = hex;

  // Remove colons if present
  hex_clean.erase(std::remove(hex_clean.begin(), hex_clean.end(), ':'), hex_clean.end());

  // Convert pairs of hex chars to bytes
  for (size_t i = 0; i < hex_clean.length(); i += 2) {
    std::string byte_string = hex_clean.substr(i, 2);
    uint8_t byte = static_cast<uint8_t>(std::stoi(byte_string, nullptr, 16));
    bytes.push_back(byte);
  }

  return bytes;
}

saasy::shared::v1::DtlsFingerprint JsonToProto(const nlohmann::json& json,
                                                saasy::shared::v1::DtlsFingerprint*) {
  saasy::shared::v1::DtlsFingerprint fingerprint;

  if (!json.contains("algorithm") || !json["algorithm"].is_string()) {
    throw std::runtime_error("DtlsFingerprint missing required field: algorithm");
  }
  fingerprint.set_algorithm(StringToDtlsFingerprintAlgorithm(json["algorithm"].get<std::string>()));

  if (!json.contains("value") || !json["value"].is_string()) {
    throw std::runtime_error("DtlsFingerprint missing required field: value");
  }
  fingerprint.set_value(HexStringToBytes(json["value"].get<std::string>()));

  return fingerprint;
}

saasy::shared::v1::DtlsParameters JsonToProto(const nlohmann::json& json,
                                               saasy::shared::v1::DtlsParameters*) {
  saasy::shared::v1::DtlsParameters dtls_params;

  if (json.contains("role") && json["role"].is_string()) {
    dtls_params.set_role(StringToDtlsRole(json["role"].get<std::string>()));
  }

  if (!json.contains("fingerprints") || !json["fingerprints"].is_array()) {
    throw std::runtime_error("DtlsParameters missing required field: fingerprints");
  }

  if (json["fingerprints"].empty()) {
    throw std::runtime_error("DtlsParameters fingerprints array cannot be empty");
  }

  for (const auto& fp_json : json["fingerprints"]) {
    *dtls_params.add_fingerprints() =
        JsonToProto(fp_json, (saasy::shared::v1::DtlsFingerprint*)nullptr);
  }

  return dtls_params;
}

saasy::shared::v1::RtpHeaderExtensionUri StringToRtpHeaderExtensionUri(const std::string& uri) {
  if (uri == "urn:ietf:params:rtp-hdrext:sdes:mid") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_MID;
  } else if (uri == "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_RTP_STREAM_ID;
  } else if (uri == "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_REPAIR_RTP_STREAM_ID;
  } else if (uri == "urn:ietf:params:rtp-hdrext:ssrc-audio-level") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_AUDIO_LEVEL;
  } else if (uri == "urn:3gpp:video-orientation") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_VIDEO_ORIENTATION;
  } else if (uri == "urn:ietf:params:rtp-hdrext:toffset") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_TIME_OFFSET;
  } else if (uri == "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_TRANSPORT_WIDE_CC_DRAFT01;
  } else if (uri == "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_ABS_SEND_TIME;
  } else if (uri == "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_ABS_CAPTURE_TIME;
  } else if (uri == "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_PLAYOUT_DELAY;
  } else if (uri == "https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_DEPENDENCY_DESCRIPTOR;
  } else {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_URI_UNSPECIFIED;
  }
}

saasy::shared::v1::RtpHeaderExtensionDirection StringToRtpHeaderExtensionDirection(
    const std::string& direction) {
  if (direction == "sendrecv") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_SEND_RECV;
  } else if (direction == "sendonly") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_SEND_ONLY;
  } else if (direction == "recvonly") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_RECV_ONLY;
  } else if (direction == "inactive") {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_INACTIVE;
  } else {
    return saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_UNSPECIFIED;
  }
}

saasy::shared::v1::MediaKind StringToMediaKind(const std::string& kind) {
  if (kind == "audio") {
    return saasy::shared::v1::MEDIA_KIND_AUDIO;
  } else if (kind == "video") {
    return saasy::shared::v1::MEDIA_KIND_VIDEO;
  } else {
    return saasy::shared::v1::MEDIA_KIND_UNSPECIFIED;
  }
}

saasy::shared::v1::RtpHeaderExtension JsonToProto(const nlohmann::json& json,
                                                   saasy::shared::v1::RtpHeaderExtension*) {
  saasy::shared::v1::RtpHeaderExtension header_ext;

  if (!json.contains("uri") || !json["uri"].is_string()) {
    throw std::runtime_error("RtpHeaderExtension missing required field: uri");
  }
  header_ext.set_uri(StringToRtpHeaderExtensionUri(json["uri"].get<std::string>()));

  if (!json.contains("preferredId") || !json["preferredId"].is_number()) {
    throw std::runtime_error("RtpHeaderExtension missing required field: preferredId");
  }
  header_ext.set_preferred_id(json["preferredId"].get<uint32_t>());

  if (!json.contains("kind") || !json["kind"].is_string()) {
    throw std::runtime_error("RtpHeaderExtension missing required field: kind");
  }
  header_ext.set_kind(StringToMediaKind(json["kind"].get<std::string>()));

  if (json.contains("preferredEncrypt") && json["preferredEncrypt"].is_boolean()) {
    header_ext.set_preferred_encrypt(json["preferredEncrypt"].get<bool>());
  }

  if (json.contains("direction") && json["direction"].is_string()) {
    header_ext.set_direction(
        StringToRtpHeaderExtensionDirection(json["direction"].get<std::string>()));
  }

  return header_ext;
}

saasy::shared::v1::MimeTypeAudio StringToMimeTypeAudio(const std::string& mime_type) {
  if (mime_type == "audio/opus") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_OPUS;
  } else if (mime_type == "audio/multiopus") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_MULTI_CHANNEL_OPUS;
  } else if (mime_type == "audio/PCMU") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_PCMU;
  } else if (mime_type == "audio/PCMA") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_PCMA;
  } else if (mime_type == "audio/ISAC") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_ISAC;
  } else if (mime_type == "audio/G722") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_G722;
  } else if (mime_type == "audio/iLBC") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_ILBC;
  } else if (mime_type == "audio/SILK") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_SILK;
  } else if (mime_type == "audio/CN") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_CN;
  } else if (mime_type == "audio/telephone-event") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_TELEPHONE_EVENT;
  } else if (mime_type == "audio/rtx") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_RTX;
  } else if (mime_type == "audio/red") {
    return saasy::shared::v1::MIME_TYPE_AUDIO_RED;
  } else {
    return saasy::shared::v1::MIME_TYPE_AUDIO_UNSPECIFIED;
  }
}

saasy::shared::v1::MimeTypeVideo StringToMimeTypeVideo(const std::string& mime_type) {
  if (mime_type == "video/VP8") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_VP8;
  } else if (mime_type == "video/VP9") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_VP9;
  } else if (mime_type == "video/H264") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_H264;
  } else if (mime_type == "video/Av1") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_AV1;
  } else if (mime_type == "video/rtx") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_RTX;
  } else if (mime_type == "video/red") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_RED;
  } else if (mime_type == "video/ulpfec") {
    return saasy::shared::v1::MIME_TYPE_VIDEO_ULPFEC;
  } else {
    return saasy::shared::v1::MIME_TYPE_VIDEO_UNSPECIFIED;
  }
}

saasy::shared::v1::RtpCodecParametersParameters JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::RtpCodecParametersParameters*) {
  saasy::shared::v1::RtpCodecParametersParameters param;

  // The JSON should be a single key-value pair object
  if (json.is_object() && json.size() == 1) {
    auto it = json.begin();
    param.set_key(it.key());

    if (it.value().is_string()) {
      param.set_string_value(it.value().get<std::string>());
    } else if (it.value().is_number()) {
      param.set_number_value(it.value().get<uint32_t>());
    } else if (it.value().is_number()) {
      // Handle signed numbers by converting to unsigned
      param.set_number_value(static_cast<uint32_t>(it.value().get<int>()));
    }
  }

  return param;
}

void JsonObjectToParameters(
    const nlohmann::json& json,
    google::protobuf::RepeatedPtrField<saasy::shared::v1::RtpCodecParametersParameters>* params) {
  if (!json.is_object()) return;

  for (auto it = json.begin(); it != json.end(); ++it) {
    auto* param = params->Add();
    param->set_key(it.key());

    if (it.value().is_string()) {
      param->set_string_value(it.value().get<std::string>());
    } else if (it.value().is_number()) {
      param->set_number_value(it.value().get<uint32_t>());
    } else if (it.value().is_number()) {
      // Handle signed numbers by converting to unsigned
      param->set_number_value(static_cast<uint32_t>(it.value().get<int>()));
    }
  }
}

saasy::shared::v1::RtcpFeedback JsonToProto(const nlohmann::json& json,
                                             saasy::shared::v1::RtcpFeedback*) {
  saasy::shared::v1::RtcpFeedback feedback;

  if (!json.contains("type") || !json["type"].is_string()) {
    throw std::runtime_error("RtcpFeedback missing required field: type");
  }
  feedback.set_type(json["type"].get<std::string>());

  if (json.contains("parameter") && json["parameter"].is_string()) {
    feedback.set_parameter(json["parameter"].get<std::string>());
  }

  return feedback;
}

saasy::shared::v1::AudioRtpCodecCapability JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::AudioRtpCodecCapability*) {
  saasy::shared::v1::AudioRtpCodecCapability codec;

  if (json.contains("preferredPayloadType") && json["preferredPayloadType"].is_number()) {
    codec.set_preferred_payload_type(json["preferredPayloadType"].get<uint32_t>());
  }

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("AudioRtpCodecCapability missing required field: mimeType");
  }
  codec.set_mime_type(StringToMimeTypeAudio(json["mimeType"].get<std::string>()));

  if (!json.contains("clockRate") || !json["clockRate"].is_number()) {
    throw std::runtime_error("AudioRtpCodecCapability missing required field: clockRate");
  }
  codec.set_clock_rate(json["clockRate"].get<uint32_t>());

  codec.set_channels(json.value("channels", 1u));  // Default to 1 for audio

  if (json.contains("parameters") && json["parameters"].is_object()) {
    JsonObjectToParameters(json["parameters"], codec.mutable_parameters());
  }

  if (json.contains("rtcpFeedback") && json["rtcpFeedback"].is_array()) {
    for (const auto& fb_json : json["rtcpFeedback"]) {
      *codec.add_rtcp_feedback() = JsonToProto(fb_json, (saasy::shared::v1::RtcpFeedback*)nullptr);
    }
  }

  return codec;
}

saasy::shared::v1::VideoRtpCodecCapability JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::VideoRtpCodecCapability*) {
  saasy::shared::v1::VideoRtpCodecCapability codec;

  if (json.contains("preferredPayloadType") && json["preferredPayloadType"].is_number()) {
    codec.set_preferred_payload_type(json["preferredPayloadType"].get<uint32_t>());
  }

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("VideoRtpCodecCapability missing required field: mimeType");
  }
  codec.set_mime_type(StringToMimeTypeVideo(json["mimeType"].get<std::string>()));

  if (!json.contains("clockRate") || !json["clockRate"].is_number()) {
    throw std::runtime_error("VideoRtpCodecCapability missing required field: clockRate");
  }
  codec.set_clock_rate(json["clockRate"].get<uint32_t>());

  if (json.contains("parameters") && json["parameters"].is_object()) {
    JsonObjectToParameters(json["parameters"], codec.mutable_parameters());
  }

  if (json.contains("rtcpFeedback") && json["rtcpFeedback"].is_array()) {
    for (const auto& fb_json : json["rtcpFeedback"]) {
      *codec.add_rtcp_feedback() = JsonToProto(fb_json, (saasy::shared::v1::RtcpFeedback*)nullptr);
    }
  }

  return codec;
}

saasy::shared::v1::RtpCodecCapability JsonToProto(const nlohmann::json& json,
                                                   saasy::shared::v1::RtpCodecCapability*) {
  saasy::shared::v1::RtpCodecCapability codec_cap;

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("RtpCodecCapability missing required field: mimeType");
  }

  std::string mime_type = json["mimeType"].get<std::string>();
  if (mime_type.find("audio/") == 0) {
    *codec_cap.mutable_audio() =
        JsonToProto(json, (saasy::shared::v1::AudioRtpCodecCapability*)nullptr);
  } else if (mime_type.find("video/") == 0) {
    *codec_cap.mutable_video() =
        JsonToProto(json, (saasy::shared::v1::VideoRtpCodecCapability*)nullptr);
  } else {
    throw std::runtime_error("RtpCodecCapability unknown mimeType: " + mime_type);
  }

  return codec_cap;
}

saasy::shared::v1::RtpCapabilities JsonToProto(const nlohmann::json& json,
                                                saasy::shared::v1::RtpCapabilities*) {
  saasy::shared::v1::RtpCapabilities rtp_caps;

  if (!json.contains("codecs") || !json["codecs"].is_array()) {
    throw std::runtime_error("RtpCapabilities missing required field: codecs");
  }
  for (const auto& codec_json : json["codecs"]) {
    *rtp_caps.add_codecs() =
        JsonToProto(codec_json, (saasy::shared::v1::RtpCodecCapability*)nullptr);
  }

  if (!json.contains("headerExtensions") || !json["headerExtensions"].is_array()) {
    throw std::runtime_error("RtpCapabilities missing required field: headerExtensions");
  }
  for (const auto& ext_json : json["headerExtensions"]) {
    *rtp_caps.add_header_extensions() =
        JsonToProto(ext_json, (saasy::shared::v1::RtpHeaderExtension*)nullptr);
  }

  return rtp_caps;
}

saasy::shared::v1::AudioRtpCodecCapabilityFinalized JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::AudioRtpCodecCapabilityFinalized*) {
  saasy::shared::v1::AudioRtpCodecCapabilityFinalized codec;

  if (!json.contains("preferredPayloadType") ||
      !json["preferredPayloadType"].is_number()) {
    throw std::runtime_error(
        "AudioRtpCodecCapabilityFinalized missing required field: preferredPayloadType");
  }
  codec.set_preferred_payload_type(json["preferredPayloadType"].get<uint32_t>());

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("AudioRtpCodecCapabilityFinalized missing required field: mimeType");
  }
  codec.set_mime_type(StringToMimeTypeAudio(json["mimeType"].get<std::string>()));

  if (!json.contains("clockRate") || !json["clockRate"].is_number()) {
    throw std::runtime_error("AudioRtpCodecCapabilityFinalized missing required field: clockRate");
  }
  codec.set_clock_rate(json["clockRate"].get<uint32_t>());

  codec.set_channels(json.value("channels", 1u));  // Default to 1 for audio

  if (json.contains("parameters") && json["parameters"].is_object()) {
    JsonObjectToParameters(json["parameters"], codec.mutable_parameters());
  }

  if (json.contains("rtcpFeedback") && json["rtcpFeedback"].is_array()) {
    for (const auto& fb_json : json["rtcpFeedback"]) {
      *codec.add_rtcp_feedback() = JsonToProto(fb_json, (saasy::shared::v1::RtcpFeedback*)nullptr);
    }
  }

  return codec;
}

saasy::shared::v1::VideoRtpCodecCapabilityFinalized JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::VideoRtpCodecCapabilityFinalized*) {
  saasy::shared::v1::VideoRtpCodecCapabilityFinalized codec;

  if (!json.contains("preferredPayloadType") ||
      !json["preferredPayloadType"].is_number()) {
    throw std::runtime_error(
        "VideoRtpCodecCapabilityFinalized missing required field: preferredPayloadType");
  }
  codec.set_preferred_payload_type(json["preferredPayloadType"].get<uint32_t>());

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("VideoRtpCodecCapabilityFinalized missing required field: mimeType");
  }
  codec.set_mime_type(StringToMimeTypeVideo(json["mimeType"].get<std::string>()));

  if (!json.contains("clockRate") || !json["clockRate"].is_number()) {
    throw std::runtime_error("VideoRtpCodecCapabilityFinalized missing required field: clockRate");
  }
  codec.set_clock_rate(json["clockRate"].get<uint32_t>());

  if (json.contains("parameters") && json["parameters"].is_object()) {
    JsonObjectToParameters(json["parameters"], codec.mutable_parameters());
  }

  if (json.contains("rtcpFeedback") && json["rtcpFeedback"].is_array()) {
    for (const auto& fb_json : json["rtcpFeedback"]) {
      *codec.add_rtcp_feedback() = JsonToProto(fb_json, (saasy::shared::v1::RtcpFeedback*)nullptr);
    }
  }

  return codec;
}

saasy::shared::v1::RtpCodecCapabilityFinalized JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::RtpCodecCapabilityFinalized*) {
  saasy::shared::v1::RtpCodecCapabilityFinalized codec_cap;

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("RtpCodecCapabilityFinalized missing required field: mimeType");
  }

  std::string mime_type = json["mimeType"].get<std::string>();
  if (mime_type.find("audio/") == 0) {
    *codec_cap.mutable_audio() =
        JsonToProto(json, (saasy::shared::v1::AudioRtpCodecCapabilityFinalized*)nullptr);
  } else if (mime_type.find("video/") == 0) {
    *codec_cap.mutable_video() =
        JsonToProto(json, (saasy::shared::v1::VideoRtpCodecCapabilityFinalized*)nullptr);
  } else {
    throw std::runtime_error("RtpCodecCapabilityFinalized unknown mimeType: " + mime_type);
  }

  return codec_cap;
}

saasy::shared::v1::RtpCapabilitiesFinalized JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::RtpCapabilitiesFinalized*) {
  saasy::shared::v1::RtpCapabilitiesFinalized rtp_caps;

  if (!json.contains("codecs") || !json["codecs"].is_array()) {
    throw std::runtime_error("RtpCapabilitiesFinalized missing required field: codecs");
  }
  for (const auto& codec_json : json["codecs"]) {
    *rtp_caps.add_codecs() =
        JsonToProto(codec_json, (saasy::shared::v1::RtpCodecCapabilityFinalized*)nullptr);
  }

  if (!json.contains("headerExtensions") || !json["headerExtensions"].is_array()) {
    throw std::runtime_error("RtpCapabilitiesFinalized missing required field: headerExtensions");
  }
  for (const auto& ext_json : json["headerExtensions"]) {
    *rtp_caps.add_header_extensions() =
        JsonToProto(ext_json, (saasy::shared::v1::RtpHeaderExtension*)nullptr);
  }

  return rtp_caps;
}

saasy::shared::v1::AudioRtpCodecParameters JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::AudioRtpCodecParameters*) {
  saasy::shared::v1::AudioRtpCodecParameters codec;

  if (!json.contains("payloadType") || !json["payloadType"].is_number()) {
    throw std::runtime_error("AudioRtpCodecParameters missing required field: payloadType");
  }
  codec.set_payload_type(json["payloadType"].get<uint32_t>());

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("AudioRtpCodecParameters missing required field: mimeType");
  }
  codec.set_mime_type(StringToMimeTypeAudio(json["mimeType"].get<std::string>()));

  if (!json.contains("clockRate") || !json["clockRate"].is_number()) {
    throw std::runtime_error("AudioRtpCodecParameters missing required field: clockRate");
  }
  codec.set_clock_rate(json["clockRate"].get<uint32_t>());

  codec.set_channels(json.value("channels", 1u));

  if (json.contains("parameters") && json["parameters"].is_object()) {
    JsonObjectToParameters(json["parameters"], codec.mutable_parameters());
  }

  if (json.contains("rtcpFeedback") && json["rtcpFeedback"].is_array()) {
    for (const auto& fb_json : json["rtcpFeedback"]) {
      *codec.add_rtcp_feedback() = JsonToProto(fb_json, (saasy::shared::v1::RtcpFeedback*)nullptr);
    }
  }

  return codec;
}

saasy::shared::v1::VideoRtpCodecParameters JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::VideoRtpCodecParameters*) {
  saasy::shared::v1::VideoRtpCodecParameters codec;

  if (!json.contains("payloadType") || !json["payloadType"].is_number()) {
    throw std::runtime_error("VideoRtpCodecParameters missing required field: payloadType");
  }
  codec.set_payload_type(json["payloadType"].get<uint32_t>());

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("VideoRtpCodecParameters missing required field: mimeType");
  }
  codec.set_mime_type(StringToMimeTypeVideo(json["mimeType"].get<std::string>()));

  if (!json.contains("clockRate") || !json["clockRate"].is_number()) {
    throw std::runtime_error("VideoRtpCodecParameters missing required field: clockRate");
  }
  codec.set_clock_rate(json["clockRate"].get<uint32_t>());

  if (json.contains("parameters") && json["parameters"].is_object()) {
    JsonObjectToParameters(json["parameters"], codec.mutable_parameters());
  }

  if (json.contains("rtcpFeedback") && json["rtcpFeedback"].is_array()) {
    for (const auto& fb_json : json["rtcpFeedback"]) {
      *codec.add_rtcp_feedback() = JsonToProto(fb_json, (saasy::shared::v1::RtcpFeedback*)nullptr);
    }
  }

  return codec;
}

saasy::shared::v1::RtpCodecParameters JsonToProto(const nlohmann::json& json,
                                                   saasy::shared::v1::RtpCodecParameters*) {
  saasy::shared::v1::RtpCodecParameters codec_params;

  if (!json.contains("mimeType") || !json["mimeType"].is_string()) {
    throw std::runtime_error("RtpCodecParameters missing required field: mimeType");
  }

  std::string mime_type = json["mimeType"].get<std::string>();
  if (mime_type.find("audio/") == 0) {
    *codec_params.mutable_audio() =
        JsonToProto(json, (saasy::shared::v1::AudioRtpCodecParameters*)nullptr);
  } else if (mime_type.find("video/") == 0) {
    *codec_params.mutable_video() =
        JsonToProto(json, (saasy::shared::v1::VideoRtpCodecParameters*)nullptr);
  } else {
    throw std::runtime_error("RtpCodecParameters unknown mimeType: " + mime_type);
  }

  return codec_params;
}

saasy::shared::v1::RtpEncodingParametersRtx JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::RtpEncodingParametersRtx*) {
  saasy::shared::v1::RtpEncodingParametersRtx rtx;

  if (json.contains("ssrc") && json["ssrc"].is_number()) {
    rtx.set_ssrc(json["ssrc"].get<uint32_t>());
  }

  return rtx;
}

saasy::shared::v1::RtpHeaderExtensionParameters JsonToProto(
    const nlohmann::json& json, saasy::shared::v1::RtpHeaderExtensionParameters*) {
  saasy::shared::v1::RtpHeaderExtensionParameters header_ext_params;

  if (!json.contains("id") || !json["id"].is_number()) {
    throw std::runtime_error("RtpHeaderExtensionParameters missing required field: id");
  }
  header_ext_params.set_id(json["id"].get<uint32_t>());

  if (!json.contains("uri") || !json["uri"].is_string()) {
    throw std::runtime_error("RtpHeaderExtensionParameters missing required field: uri");
  }
  header_ext_params.set_uri(StringToRtpHeaderExtensionUri(json["uri"].get<std::string>()));

  if (json.contains("encrypt") && json["encrypt"].is_boolean()) {
    header_ext_params.set_encrypt(json["encrypt"].get<bool>());
  }

  return header_ext_params;
}

saasy::shared::v1::ScalabilityModeEnum StringToScalabilityModeEnum(const std::string& mode) {
  if (mode == "L1T1") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_NONE;
  } else if (mode == "L1T2") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T2;
  } else if (mode == "L1T2h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T2H;
  } else if (mode == "L1T3") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T3;
  } else if (mode == "L1T3h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T3H;
  } else if (mode == "L2T1") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T1;
  } else if (mode == "L2T1h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T1H;
  } else if (mode == "L2T1_KEY") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T1_KEY;
  } else if (mode == "L2T2") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2;
  } else if (mode == "L2T2h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2H;
  } else if (mode == "L2T2_KEY") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2_KEY;
  } else if (mode == "L2T2_KEY_SHIFT") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2_KEY_SHIFT;
  } else if (mode == "L2T3") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3;
  } else if (mode == "L2T3h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3H;
  } else if (mode == "L2T3_KEY") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3_KEY;
  } else if (mode == "L2T3_KEY_SHIFT") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3_KEY_SHIFT;
  } else if (mode == "L3T1") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T1;
  } else if (mode == "L3T1h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T1H;
  } else if (mode == "L3T1_KEY") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T1_KEY;
  } else if (mode == "L3T2") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2;
  } else if (mode == "L3T2h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2H;
  } else if (mode == "L3T2_KEY") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2_KEY;
  } else if (mode == "L3T2_KEY_SHIFT") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2_KEY_SHIFT;
  } else if (mode == "L3T3") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3;
  } else if (mode == "L3T3h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3H;
  } else if (mode == "L3T3_KEY") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3_KEY;
  } else if (mode == "L3T3_KEY_SHIFT") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3_KEY_SHIFT;
  } else if (mode == "S2T1") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T1;
  } else if (mode == "S2T1h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T1H;
  } else if (mode == "S2T2") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T2;
  } else if (mode == "S2T2h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T2H;
  } else if (mode == "S2T3") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T3;
  } else if (mode == "S2T3h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T3H;
  } else if (mode == "S3T1") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T1;
  } else if (mode == "S3T1h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T1H;
  } else if (mode == "S3T2") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T2;
  } else if (mode == "S3T2h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T2H;
  } else if (mode == "S3T3") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T3;
  } else if (mode == "S3T3h") {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T3H;
  } else {
    return saasy::shared::v1::SCALABILITY_MODE_ENUM_UNSPECIFIED;
  }
}

saasy::shared::v1::ScalabilityModeCustom JsonToProto(const nlohmann::json& json,
                                                      saasy::shared::v1::ScalabilityModeCustom*) {
  saasy::shared::v1::ScalabilityModeCustom custom_mode;

  if (!json.contains("scalabilityMode") || !json["scalabilityMode"].is_string()) {
    throw std::runtime_error("ScalabilityModeCustom missing required field: scalabilityMode");
  }
  custom_mode.set_scalability_mode(json["scalabilityMode"].get<std::string>());

  if (!json.contains("spatialLayers") || !json["spatialLayers"].is_number()) {
    throw std::runtime_error("ScalabilityModeCustom missing required field: spatialLayers");
  }
  uint32_t spatial = json["spatialLayers"].get<uint32_t>();
  if (spatial == 0) {
    throw std::runtime_error("ScalabilityModeCustom spatialLayers must be non-zero");
  }
  custom_mode.set_spatial_layers(spatial);

  if (!json.contains("temporalLayers") || !json["temporalLayers"].is_number()) {
    throw std::runtime_error("ScalabilityModeCustom missing required field: temporalLayers");
  }
  uint32_t temporal = json["temporalLayers"].get<uint32_t>();
  if (temporal == 0) {
    throw std::runtime_error("ScalabilityModeCustom temporalLayers must be non-zero");
  }
  custom_mode.set_temporal_layers(temporal);

  if (!json.contains("ksvc") || !json["ksvc"].is_boolean()) {
    throw std::runtime_error("ScalabilityModeCustom missing required field: ksvc");
  }
  custom_mode.set_ksvc(json["ksvc"].get<bool>());

  return custom_mode;
}

saasy::shared::v1::ScalabilityMode JsonToProto(const nlohmann::json& json,
                                                saasy::shared::v1::ScalabilityMode*) {
  saasy::shared::v1::ScalabilityMode scalability_mode;

  if (json.is_string()) {
    scalability_mode.set_predefined(StringToScalabilityModeEnum(json.get<std::string>()));
  } else if (json.is_object()) {
    *scalability_mode.mutable_custom() =
        JsonToProto(json, (saasy::shared::v1::ScalabilityModeCustom*)nullptr);
  } else {
    scalability_mode.set_predefined(saasy::shared::v1::SCALABILITY_MODE_ENUM_NONE);
  }

  return scalability_mode;
}

saasy::shared::v1::RtpEncodingParameters JsonToProto(const nlohmann::json& json,
                                                      saasy::shared::v1::RtpEncodingParameters*) {
  saasy::shared::v1::RtpEncodingParameters encoding;

  if (json.contains("ssrc") && json["ssrc"].is_number()) {
    encoding.set_ssrc(json["ssrc"].get<uint32_t>());
  }

  if (json.contains("rid") && json["rid"].is_string()) {
    encoding.set_rid(json["rid"].get<std::string>());
  }

  if (json.contains("codecPayloadType") && json["codecPayloadType"].is_number()) {
    encoding.set_codec_payload_type(json["codecPayloadType"].get<uint32_t>());
  }

  if (json.contains("dtx") && json["dtx"].is_boolean()) {
    encoding.set_dtx(json["dtx"].get<bool>());
  }

  if (json.contains("maxBitrate") && json["maxBitrate"].is_number()) {
    encoding.set_max_bitrate(json["maxBitrate"].get<uint32_t>());
  }

  if (json.contains("rtx") && json["rtx"].is_object()) {
    *encoding.mutable_rtx() =
        JsonToProto(json["rtx"], (saasy::shared::v1::RtpEncodingParametersRtx*)nullptr);
  }

  if (json.contains("scalabilityMode")) {
    *encoding.mutable_scalability_mode() =
        JsonToProto(json["scalabilityMode"], (saasy::shared::v1::ScalabilityMode*)nullptr);
  }

  return encoding;
}

saasy::shared::v1::RtcpParameters JsonToProto(const nlohmann::json& json,
                                               saasy::shared::v1::RtcpParameters*) {
  saasy::shared::v1::RtcpParameters rtcp;

  if (json.contains("cname") && json["cname"].is_string()) {
    rtcp.set_cname(json["cname"].get<std::string>());
  }

  if (json.contains("reducedSize") && json["reducedSize"].is_boolean()) {
    rtcp.set_reduced_size(json["reducedSize"].get<bool>());
  } else {
    // Default to false if not specified
    rtcp.set_reduced_size(false);
  }

  return rtcp;
}

saasy::shared::v1::RtpParameters JsonToProto(const nlohmann::json& json,
                                              saasy::shared::v1::RtpParameters*) {
  saasy::shared::v1::RtpParameters rtp_params;

  if (json.contains("mid") && json["mid"].is_string()) {
    rtp_params.set_mid(json["mid"].get<std::string>());
  }

  if (!json.contains("codecs") || !json["codecs"].is_array()) {
    throw std::runtime_error("RtpParameters missing required field: codecs");
  }
  for (const auto& codec_json : json["codecs"]) {
    *rtp_params.add_codecs() =
        JsonToProto(codec_json, (saasy::shared::v1::RtpCodecParameters*)nullptr);
  }

  if (json.contains("headerExtensions") && json["headerExtensions"].is_array()) {
    for (const auto& ext_json : json["headerExtensions"]) {
      *rtp_params.add_header_extensions() =
          JsonToProto(ext_json, (saasy::shared::v1::RtpHeaderExtensionParameters*)nullptr);
    }
  }

  if (json.contains("encodings") && json["encodings"].is_array()) {
    for (const auto& encoding_json : json["encodings"]) {
      *rtp_params.add_encodings() =
          JsonToProto(encoding_json, (saasy::shared::v1::RtpEncodingParameters*)nullptr);
    }
  }

  if (json.contains("rtcp") && json["rtcp"].is_object()) {
    *rtp_params.mutable_rtcp() =
        JsonToProto(json["rtcp"], (saasy::shared::v1::RtcpParameters*)nullptr);
  }

  return rtp_params;
}

}  // namespace saasy::common
