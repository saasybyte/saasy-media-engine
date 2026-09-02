#pragma once

#include <json.hpp>

#include "protos/shared/v1/shared.pb.h"

namespace saasy::common {

saasy::shared::v1::DtlsParameters JsonToProto(const nlohmann::json& json,
                                               saasy::shared::v1::DtlsParameters*);

saasy::shared::v1::RtpParameters JsonToProto(const nlohmann::json& json,
                                              saasy::shared::v1::RtpParameters*);

saasy::shared::v1::RtpCapabilities JsonToProto(const nlohmann::json& json,
                                                saasy::shared::v1::RtpCapabilities*);

}  // namespace saasy::common