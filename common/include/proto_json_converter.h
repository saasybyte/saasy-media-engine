#pragma once

#include <json.hpp>

#include "protos/shared/v1/shared.pb.h"

namespace saasy::common {

nlohmann::json ProtoToJson(const saasy::shared::v1::IceParameters& ice_params);

nlohmann::json ProtoToJson(const saasy::shared::v1::IceCandidate& candidate);

nlohmann::json ProtoToJson(const saasy::shared::v1::DtlsParameters& dtls_params);

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpCapabilitiesFinalized& rtp_caps);

nlohmann::json ProtoToJson(const saasy::shared::v1::RtpParameters& rtp_params);

}  // namespace saasy::common
