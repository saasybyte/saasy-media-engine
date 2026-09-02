#pragma once

#include <memory>
#include <mutex>

#include "webrtc/api/scoped_refptr.h"
#include "webrtc/rtc_base/thread.h"

namespace webrtc {
class PeerConnectionFactoryInterface;

class AudioDeviceModule;

}  // namespace webrtc

namespace saasy::common {

class WebRTCFactory {
 public:
  static webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> CreateFactory(
      webrtc::scoped_refptr<webrtc::AudioDeviceModule> adm = nullptr);

  static void Initialize();

  static void Shutdown();

 private:
  WebRTCFactory() = default;

  ~WebRTCFactory() = default;

  static std::mutex mutex_;
  static std::unique_ptr<webrtc::Thread> network_thread_;
  static std::unique_ptr<webrtc::Thread> signaling_thread_;
  static std::unique_ptr<webrtc::Thread> worker_thread_;
  static bool initialized_;
};

}  // namespace saasy::common
