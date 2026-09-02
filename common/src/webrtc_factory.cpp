#include "webrtc_factory.h"

#include <iostream>

#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/create_peerconnection_factory.h"

namespace saasy::common {

// Static member definitions
std::mutex WebRTCFactory::mutex_;
std::unique_ptr<webrtc::Thread> WebRTCFactory::network_thread_;
std::unique_ptr<webrtc::Thread> WebRTCFactory::signaling_thread_;
std::unique_ptr<webrtc::Thread> WebRTCFactory::worker_thread_;
bool WebRTCFactory::initialized_ = false;

void WebRTCFactory::Initialize() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (initialized_) {
    std::cout << "[WebRTCFactory] Already initialized\n";
    return;
  }

  std::cout << "[WebRTCFactory] Initializing threads\n";

  // Create network thread
  network_thread_ = webrtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("network_thread", nullptr);
  network_thread_->Start();

  // Create worker thread
  worker_thread_ = webrtc::Thread::Create();
  worker_thread_->SetName("worker_thread", nullptr);
  worker_thread_->Start();

  // Create signaling thread
  signaling_thread_ = webrtc::Thread::Create();
  signaling_thread_->SetName("signaling_thread", nullptr);
  signaling_thread_->Start();

  initialized_ = true;
  std::cout << "[WebRTCFactory] Threads initialized\n";
}

webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> WebRTCFactory::CreateFactory(
    webrtc::scoped_refptr<webrtc::AudioDeviceModule> adm) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_) {
    std::cerr << "[WebRTCFactory] Not initialized! Call Initialize() first\n";
    return nullptr;
  }

  std::cout << "[WebRTCFactory] Creating PeerConnectionFactory"
            << (adm ? " with custom ADM" : " with default ADM") << "\n";

  auto factory = webrtc::CreatePeerConnectionFactory(
      network_thread_.get(), worker_thread_.get(), signaling_thread_.get(),
      adm,  // Can be nullptr for default ADM
      webrtc::CreateBuiltinAudioEncoderFactory(), webrtc::CreateBuiltinAudioDecoderFactory(),
      nullptr,  // video_encoder_factory
      nullptr,  // video_decoder_factory
      nullptr,  // audio_mixer
      nullptr   // audio_processing
  );

  if (!factory) {
    std::cerr << "[WebRTCFactory] Failed to create PeerConnectionFactory\n";
  }

  return factory;
}

void WebRTCFactory::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!initialized_) {
    return;
  }

  std::cout << "[WebRTCFactory] Shutting down\n";

  // Stop threads in reverse order
  if (signaling_thread_) {
    signaling_thread_->Stop();
    signaling_thread_.reset();
  }

  if (worker_thread_) {
    worker_thread_->Stop();
    worker_thread_.reset();
  }

  if (network_thread_) {
    network_thread_->Stop();
    network_thread_.reset();
  }

  initialized_ = false;
  std::cout << "[WebRTCFactory] Shutdown complete\n";
}

}  // namespace saasy::common
