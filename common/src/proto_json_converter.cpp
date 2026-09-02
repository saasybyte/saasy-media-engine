#include "proto_json_converter.h"

#include <cstdio>

namespace saasy::common {

nlohmann::json ProtoToJson(const saasy::shared::v1::IceParameters& ice_params) {
  return nlohmann::json{{"usernameFragment", ice_params.username_fragment()},
                        {"password", ice_params.password()},
                        {"iceLite", ice_params.has_ice_lite() ? ice_params.ice_lite() : false}};
}

std::string ProtocolToString(saasy::shared::v1::Protocol protocol) {
  switch (protocol) {
    case saasy::shared::v1::PROTOCOL_TCP:
      return "tcp";
    case saasy::shared::v1::PROTOCOL_UDP:
      return "udp";
    default:
      return "udp";
  }
}

std::string IceCandidateTypeToString(saasy::shared::v1::IceCandidateType type) {
  switch (type) {
    case saasy::shared::v1::ICE_CANDIDATE_TYPE_HOST:
      return "host";
    case saasy::shared::v1::ICE_CANDIDATE_TYPE_SRFLX:
      return "srflx";
    case saasy::shared::v1::ICE_CANDIDATE_TYPE_PRFLX:
      return "prflx";
    case saasy::shared::v1::ICE_CANDIDATE_TYPE_RELAY:
      return "relay";
    default:
      return "host";
  }
}

std::string IceCandidateTcpTypeToString(saasy::shared::v1::IceCandidateTcpType tcp_type) {
  switch (tcp_type) {
    case saasy::shared::v1::ICE_CANDIDATE_TCP_TYPE_PASSIVE:
      return "passive";
    default:
      return "passive";
  }
}

nlohmann::json ProtoToJson(const saasy::shared::v1::IceCandidate& ice_candidate) {
  nlohmann::json json_ice_candidate = {{"foundation", ice_candidate.foundation()},
                                       {"priority", ice_candidate.priority()},
                                       {"ip", ice_candidate.address()},
                                       {"protocol", ProtocolToString(ice_candidate.protocol())},
                                       {"port", ice_candidate.port()},
                                       {"type", IceCandidateTypeToString(ice_candidate.type())}};

  if (ice_candidate.has_tcp_type()) {
    json_ice_candidate["tcpType"] = IceCandidateTcpTypeToString(ice_candidate.tcp_type());
  }

  return json_ice_candidate;
}

std::string DtlsRoleToString(saasy::shared::v1::DtlsRole role) {
  switch (role) {
    case saasy::shared::v1::DTLS_ROLE_AUTO:
      return "auto";
    case saasy::shared::v1::DTLS_ROLE_CLIENT:
      return "client";
    case saasy::shared::v1::DTLS_ROLE_SERVER:
      return "server";
    default:
      return "auto";
  }
}

std::string DtlsFingerprintAlgorithmToString(
    saasy::shared::v1::DtlsFingerprintAlgorithm algorithm) {
  switch (algorithm) {
    case saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA1:
      return "sha-1";
    case saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA224:
      return "sha-224";
    case saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA256:
      return "sha-256";
    case saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA384:
      return "sha-384";
    case saasy::shared::v1::DTLS_FINGERPRINT_ALGORITHM_SHA512:
      return "sha-512";
    default:
      return "sha-256";
  }
}

std::string BytesToHexString(const std::string& bytes) {
  std::string hex_value;
  for (unsigned char c : bytes) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X:", static_cast<unsigned>(c));
    hex_value += buf;
  }
  if (!hex_value.empty()) {
    hex_value.pop_back();  // Remove trailing colon
  }
  return hex_value;
}

nlohmann::json ProtoToJson(const saasy::shared::v1::DtlsFingerprint& fingerprint) {
  return nlohmann::json{{"algorithm", DtlsFingerprintAlgorithmToString(fingerprint.algorithm())},
                        {"value", BytesToHexString(fingerprint.value())}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::DtlsParameters& dtls_params) {
  nlohmann::json fingerprints = nlohmann::json::array();

  for (const auto& fp : dtls_params.fingerprints()) {
    fingerprints.push_back(ProtoToJson(fp));
  }

  return nlohmann::json{{"role", DtlsRoleToString(dtls_params.role())},
                        {"fingerprints", fingerprints}};
}

std::string RtpHeaderExtensionUriToString(saasy::shared::v1::RtpHeaderExtensionUri uri) {
  switch (uri) {
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_MID:
      return "urn:ietf:params:rtp-hdrext:sdes:mid";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_RTP_STREAM_ID:
      return "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_REPAIR_RTP_STREAM_ID:
      return "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_AUDIO_LEVEL:
      return "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_VIDEO_ORIENTATION:
      return "urn:3gpp:video-orientation";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_TIME_OFFSET:
      return "urn:ietf:params:rtp-hdrext:toffset";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_TRANSPORT_WIDE_CC_DRAFT01:
      return "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_ABS_SEND_TIME:
      return "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_ABS_CAPTURE_TIME:
      return "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_PLAYOUT_DELAY:
      return "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_URI_DEPENDENCY_DESCRIPTOR:
      return "https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension";
    default:
      return "";
  }
}

std::string RtpHeaderExtensionDirectionToString(
    saasy::shared::v1::RtpHeaderExtensionDirection direction) {
  switch (direction) {
    case saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_SEND_RECV:
      return "sendrecv";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_SEND_ONLY:
      return "sendonly";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_RECV_ONLY:
      return "recvonly";
    case saasy::shared::v1::RTP_HEADER_EXTENSION_DIRECTION_INACTIVE:
      return "inactive";
    default:
      return "sendrecv";
  }
}

std::string MediaKindToString(saasy::shared::v1::MediaKind kind) {
  switch (kind) {
    case saasy::shared::v1::MEDIA_KIND_AUDIO:
      return "audio";
    case saasy::shared::v1::MEDIA_KIND_VIDEO:
      return "video";
    default:
      return "audio";
  }
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpHeaderExtension& header_ext) {
  return nlohmann::json{{"uri", RtpHeaderExtensionUriToString(header_ext.uri())},
                        {"preferredId", header_ext.preferred_id()},
                        {"preferredEncrypt", header_ext.preferred_encrypt()},
                        {"kind", MediaKindToString(header_ext.kind())},
                        {"direction", RtpHeaderExtensionDirectionToString(header_ext.direction())}};
}

std::string MimeTypeAudioToString(saasy::shared::v1::MimeTypeAudio mime_type) {
  switch (mime_type) {
    case saasy::shared::v1::MIME_TYPE_AUDIO_OPUS:
      return "audio/opus";
    case saasy::shared::v1::MIME_TYPE_AUDIO_MULTI_CHANNEL_OPUS:
      return "audio/multiopus";
    case saasy::shared::v1::MIME_TYPE_AUDIO_PCMU:
      return "audio/PCMU";
    case saasy::shared::v1::MIME_TYPE_AUDIO_PCMA:
      return "audio/PCMA";
    case saasy::shared::v1::MIME_TYPE_AUDIO_ISAC:
      return "audio/ISAC";
    case saasy::shared::v1::MIME_TYPE_AUDIO_G722:
      return "audio/G722";
    case saasy::shared::v1::MIME_TYPE_AUDIO_ILBC:
      return "audio/iLBC";
    case saasy::shared::v1::MIME_TYPE_AUDIO_SILK:
      return "audio/SILK";
    case saasy::shared::v1::MIME_TYPE_AUDIO_CN:
      return "audio/CN";
    case saasy::shared::v1::MIME_TYPE_AUDIO_TELEPHONE_EVENT:
      return "audio/telephone-event";
    case saasy::shared::v1::MIME_TYPE_AUDIO_RTX:
      return "audio/rtx";
    case saasy::shared::v1::MIME_TYPE_AUDIO_RED:
      return "audio/red";
    default:
      return "audio/opus";
  }
}

std::string MimeTypeVideoToString(saasy::shared::v1::MimeTypeVideo mime_type) {
  switch (mime_type) {
    case saasy::shared::v1::MIME_TYPE_VIDEO_VP8:
      return "video/VP8";
    case saasy::shared::v1::MIME_TYPE_VIDEO_VP9:
      return "video/VP9";
    case saasy::shared::v1::MIME_TYPE_VIDEO_H264:
      return "video/H264";
    case saasy::shared::v1::MIME_TYPE_VIDEO_AV1:
      return "video/Av1";
    case saasy::shared::v1::MIME_TYPE_VIDEO_RTX:
      return "video/rtx";
    case saasy::shared::v1::MIME_TYPE_VIDEO_RED:
      return "video/red";
    case saasy::shared::v1::MIME_TYPE_VIDEO_ULPFEC:
      return "video/ulpfec";
    default:
      return "video/VP8";
  }
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCodecParametersParameters& param) {
  if (param.has_string_value()) {
    return nlohmann::json{{param.key(), param.string_value()}};
  } else if (param.has_number_value()) {
    return nlohmann::json{{param.key(), param.number_value()}};
  }
  return nlohmann::json{};
}

nlohmann::json ParametersToJsonObject(
    const google::protobuf::RepeatedPtrField<saasy::shared::v1::RtpCodecParametersParameters>&
        params) {
  nlohmann::json parameters = nlohmann::json::object();
  for (const auto& param : params) {
    if (param.has_string_value()) {
      parameters[param.key()] = param.string_value();
    } else if (param.has_number_value()) {
      parameters[param.key()] = param.number_value();
    }
  }
  return parameters;
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtcpFeedback& feedback) {
  return nlohmann::json{{"type", feedback.type()}, {"parameter", feedback.parameter()}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::AudioRtpCodecCapability& codec) {
  nlohmann::json rtcp_feedback = nlohmann::json::array();
  for (const auto& fb : codec.rtcp_feedback()) {
    rtcp_feedback.push_back(ProtoToJson(fb));
  }

  nlohmann::json json_capability = {{"mimeType", MimeTypeAudioToString(codec.mime_type())},
                                    {"clockRate", codec.clock_rate()},
                                    {"channels", codec.channels()},
                                    {"parameters", ParametersToJsonObject(codec.parameters())},
                                    {"rtcpFeedback", rtcp_feedback}};

  if (codec.has_preferred_payload_type()) {
    json_capability["preferredPayloadType"] = codec.preferred_payload_type();
  }

  return json_capability;
}

nlohmann::json ProtoToJson(const saasy::shared::v1::VideoRtpCodecCapability& codec) {
  nlohmann::json rtcp_feedback = nlohmann::json::array();
  for (const auto& fb : codec.rtcp_feedback()) {
    rtcp_feedback.push_back(ProtoToJson(fb));
  }

  nlohmann::json json_capability = {{"mimeType", MimeTypeVideoToString(codec.mime_type())},
                                    {"clockRate", codec.clock_rate()},
                                    {"parameters", ParametersToJsonObject(codec.parameters())},
                                    {"rtcpFeedback", rtcp_feedback}};

  if (codec.has_preferred_payload_type()) {
    json_capability["preferredPayloadType"] = codec.preferred_payload_type();
  }

  return json_capability;
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCodecCapability& codec_cap) {
  if (codec_cap.has_audio()) {
    return ProtoToJson(codec_cap.audio());
  } else if (codec_cap.has_video()) {
    return ProtoToJson(codec_cap.video());
  }

  return nlohmann::json::object();
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCapabilities& rtp_caps) {
  nlohmann::json codecs = nlohmann::json::array();
  for (const auto& codec : rtp_caps.codecs()) {
    codecs.push_back(ProtoToJson(codec));
  }

  nlohmann::json header_extensions = nlohmann::json::array();
  for (const auto& ext : rtp_caps.header_extensions()) {
    header_extensions.push_back(ProtoToJson(ext));
  }

  return nlohmann::json{{"codecs", codecs}, {"headerExtensions", header_extensions}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::AudioRtpCodecCapabilityFinalized& codec) {
  nlohmann::json rtcp_feedback = nlohmann::json::array();
  for (const auto& fb : codec.rtcp_feedback()) {
    rtcp_feedback.push_back(ProtoToJson(fb));
  }

  return nlohmann::json{{"preferredPayloadType", codec.preferred_payload_type()},
                        {"mimeType", MimeTypeAudioToString(codec.mime_type())},
                        {"clockRate", codec.clock_rate()},
                        {"channels", codec.channels()},
                        {"parameters", ParametersToJsonObject(codec.parameters())},
                        {"rtcpFeedback", rtcp_feedback}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::VideoRtpCodecCapabilityFinalized& codec) {
  nlohmann::json rtcp_feedback = nlohmann::json::array();
  for (const auto& fb : codec.rtcp_feedback()) {
    rtcp_feedback.push_back(ProtoToJson(fb));
  }

  return nlohmann::json{{"preferredPayloadType", codec.preferred_payload_type()},
                        {"mimeType", MimeTypeVideoToString(codec.mime_type())},
                        {"clockRate", codec.clock_rate()},
                        {"parameters", ParametersToJsonObject(codec.parameters())},
                        {"rtcpFeedback", rtcp_feedback}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCodecCapabilityFinalized& codec_cap) {
  if (codec_cap.has_audio()) {
    return ProtoToJson(codec_cap.audio());
  } else if (codec_cap.has_video()) {
    return ProtoToJson(codec_cap.video());
  }

  return nlohmann::json::object();
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCapabilitiesFinalized& rtp_caps) {
  nlohmann::json codecs = nlohmann::json::array();
  for (const auto& codec : rtp_caps.codecs()) {
    codecs.push_back(ProtoToJson(codec));
  }

  nlohmann::json header_extensions = nlohmann::json::array();
  for (const auto& ext : rtp_caps.header_extensions()) {
    header_extensions.push_back(ProtoToJson(ext));
  }

  return nlohmann::json{{"codecs", codecs}, {"headerExtensions", header_extensions}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::AudioRtpCodecParameters& codec) {
  nlohmann::json rtcp_feedback = nlohmann::json::array();
  for (const auto& fb : codec.rtcp_feedback()) {
    rtcp_feedback.push_back(ProtoToJson(fb));
  }

  return nlohmann::json{{"payloadType", codec.payload_type()},
                        {"mimeType", MimeTypeAudioToString(codec.mime_type())},
                        {"clockRate", codec.clock_rate()},
                        {"channels", codec.channels()},
                        {"parameters", ParametersToJsonObject(codec.parameters())},
                        {"rtcpFeedback", rtcp_feedback}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::VideoRtpCodecParameters& codec) {
  nlohmann::json rtcp_feedback = nlohmann::json::array();
  for (const auto& fb : codec.rtcp_feedback()) {
    rtcp_feedback.push_back(ProtoToJson(fb));
  }

  return nlohmann::json{{"payloadType", codec.payload_type()},
                        {"mimeType", MimeTypeVideoToString(codec.mime_type())},
                        {"clockRate", codec.clock_rate()},
                        {"parameters", ParametersToJsonObject(codec.parameters())},
                        {"rtcpFeedback", rtcp_feedback}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCodecParameters& codec_params) {
  if (codec_params.has_audio()) {
    return ProtoToJson(codec_params.audio());
  } else if (codec_params.has_video()) {
    return ProtoToJson(codec_params.video());
  }

  return nlohmann::json::object();
}

nlohmann::json ProtoToJson(
    const saasy::shared::v1::RtpHeaderExtensionParameters& header_ext_params) {
  return nlohmann::json{{"id", header_ext_params.id()},
                        {"uri", RtpHeaderExtensionUriToString(header_ext_params.uri())},
                        {"encrypt", header_ext_params.encrypt()}};
}

std::string ScalabilityModeEnumToString(saasy::shared::v1::ScalabilityModeEnum mode) {
  switch (mode) {
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_NONE:
      return "L1T1";  // None typically maps to L1T1
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T2:
      return "L1T2";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T2H:
      return "L1T2h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T3:
      return "L1T3";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L1T3H:
      return "L1T3h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T1:
      return "L2T1";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T1H:
      return "L2T1h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T1_KEY:
      return "L2T1_KEY";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2:
      return "L2T2";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2H:
      return "L2T2h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2_KEY:
      return "L2T2_KEY";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T2_KEY_SHIFT:
      return "L2T2_KEY_SHIFT";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3:
      return "L2T3";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3H:
      return "L2T3h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3_KEY:
      return "L2T3_KEY";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L2T3_KEY_SHIFT:
      return "L2T3_KEY_SHIFT";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T1:
      return "L3T1";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T1H:
      return "L3T1h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T1_KEY:
      return "L3T1_KEY";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2:
      return "L3T2";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2H:
      return "L3T2h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2_KEY:
      return "L3T2_KEY";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T2_KEY_SHIFT:
      return "L3T2_KEY_SHIFT";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3:
      return "L3T3";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3H:
      return "L3T3h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3_KEY:
      return "L3T3_KEY";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_L3T3_KEY_SHIFT:
      return "L3T3_KEY_SHIFT";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T1:
      return "S2T1";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T1H:
      return "S2T1h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T2:
      return "S2T2";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T2H:
      return "S2T2h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T3:
      return "S2T3";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S2T3H:
      return "S2T3h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T1:
      return "S3T1";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T1H:
      return "S3T1h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T2:
      return "S3T2";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T2H:
      return "S3T2h";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T3:
      return "S3T3";
    case saasy::shared::v1::SCALABILITY_MODE_ENUM_S3T3H:
      return "S3T3h";
    default:
      return "L1T1";
  }
}

nlohmann::json ProtoToJson(const saasy::shared::v1::ScalabilityModeCustom& custom_mode) {
  return nlohmann::json{{"scalabilityMode", custom_mode.scalability_mode()},
                        {"spatialLayers", custom_mode.spatial_layers()},
                        {"temporalLayers", custom_mode.temporal_layers()},
                        {"ksvc", custom_mode.ksvc()}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::ScalabilityMode& scalability_mode) {
  if (scalability_mode.has_predefined()) {
    // For predefined modes, just return the string representation
    return ScalabilityModeEnumToString(scalability_mode.predefined());
  } else if (scalability_mode.has_custom()) {
    // For custom modes, return the full object
    return ProtoToJson(scalability_mode.custom());
  }

  return "L1T1";
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpEncodingParametersRtx& rtx) {
  return nlohmann::json{{"ssrc", rtx.ssrc()}};
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpEncodingParameters& encoding) {
  nlohmann::json json_encoding = nlohmann::json::object();

  if (encoding.has_ssrc()) {
    json_encoding["ssrc"] = encoding.ssrc();
  }

  if (encoding.has_rid()) {
    json_encoding["rid"] = encoding.rid();
  }

  if (encoding.has_codec_payload_type()) {
    json_encoding["codecPayloadType"] = encoding.codec_payload_type();
  }

  if (encoding.has_dtx()) {
    json_encoding["dtx"] = encoding.dtx();
  }

  if (encoding.has_max_bitrate()) {
    json_encoding["maxBitrate"] = encoding.max_bitrate();
  }

  if (encoding.has_rtx()) {
    json_encoding["rtx"] = ProtoToJson(encoding.rtx());
  }

  if (encoding.has_scalability_mode()) {
    json_encoding["scalabilityMode"] = ProtoToJson(encoding.scalability_mode());
  }

  return json_encoding;
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtcpParameters& rtcp) {
  nlohmann::json json_rtcp = nlohmann::json{{"reducedSize", rtcp.reduced_size()}};

  if (rtcp.has_cname()) {
    json_rtcp["cname"] = rtcp.cname();
  }

  return json_rtcp;
}

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpParameters& rtp_params) {
  nlohmann::json json_params = nlohmann::json::object();

  if (rtp_params.has_mid()) {
    json_params["mid"] = rtp_params.mid();
  }

  nlohmann::json codecs = nlohmann::json::array();
  for (const auto& codec : rtp_params.codecs()) {
    codecs.push_back(ProtoToJson(codec));
  }
  json_params["codecs"] = codecs;

  nlohmann::json header_extensions = nlohmann::json::array();
  for (const auto& ext : rtp_params.header_extensions()) {
    header_extensions.push_back(ProtoToJson(ext));
  }
  json_params["headerExtensions"] = header_extensions;

  nlohmann::json encodings = nlohmann::json::array();
  for (const auto& encoding : rtp_params.encodings()) {
    encodings.push_back(ProtoToJson(encoding));
  }
  json_params["encodings"] = encodings;

  json_params["rtcp"] = ProtoToJson(rtp_params.rtcp());

  return json_params;
}

}  // namespace saasy::common
