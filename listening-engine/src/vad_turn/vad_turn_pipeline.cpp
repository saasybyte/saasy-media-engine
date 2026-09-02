#include "vad_turn/vad_turn_pipeline.h"

#include <iostream>

#include "session.h"

namespace saasy::listening_engine {

VadTurnPipeline::VadTurnPipeline(
    const std::string& session_id,
    std::shared_ptr<SileroVAD> shared_vad,
    std::shared_ptr<SmartTurnDetector> shared_turn_detector,
    Session* session)
    : session_id_(session_id),
      session_(session),
      shared_vad_(shared_vad),
      shared_turn_detector_(shared_turn_detector),
      current_state_(VadState::kIdle) {
  
  resampler_ = std::make_unique<AudioResampler>();
  turn_buffer_ = std::make_unique<AudioTurnBuffer>();
  
  vad_accumulator_.reserve(kVadChunkSize);
  
  std::cout << "[VadTurnPipeline] Created for session: " << session_id_ << "\n";
}

VadTurnPipeline::~VadTurnPipeline() {
  Stop();
  std::cout << "[VadTurnPipeline] Destroyed for session: " << session_id_ << "\n";
}

void VadTurnPipeline::Start() {
  running_ = true;
  current_state_ = VadState::kIdle;
  silence_start_time_ = std::chrono::steady_clock::now();
  speech_start_time_ = std::chrono::steady_clock::now();
  std::cout << "[VadTurnPipeline] Started for session: " << session_id_ << "\n";
}

void VadTurnPipeline::Stop() {
  running_ = false;
  std::cout << "[VadTurnPipeline] Stopped for session: " << session_id_ << "\n";
}

void VadTurnPipeline::ProcessAudio(const int16_t* audio, size_t samples) {
  if (!running_ || !audio || samples == 0) {
    return;
  }

  // Step 1: Resample 48kHz → 16kHz
  std::vector<float> resampled_audio;
  if (!resampler_->Resample(audio, samples, resampled_audio)) {
    std::cerr << "[VadTurnPipeline] Resampling failed for session: " << session_id_ << "\n";
    return;
  }

  // Step 2: Buffer resampled audio for SmartTurn analysis
  turn_buffer_->Append(resampled_audio);

  // Step 3: Accumulate until we have 512 samples (32ms) for VAD
  std::lock_guard<std::mutex> lock(accumulator_mutex_);
  vad_accumulator_.insert(vad_accumulator_.end(), resampled_audio.begin(), resampled_audio.end());

  while (vad_accumulator_.size() >= kVadChunkSize) {
    // Extract 512 samples for VAD processing
    std::vector<float> vad_chunk(vad_accumulator_.begin(), 
                                 vad_accumulator_.begin() + kVadChunkSize);
    
    // Remove processed samples from accumulator
    vad_accumulator_.erase(vad_accumulator_.begin(), 
                          vad_accumulator_.begin() + kVadChunkSize);
    
    // Step 4: Run VAD on chunk
    ProcessVadChunk(vad_chunk);
  }
}

void VadTurnPipeline::ProcessVadChunk(const std::vector<float>& chunk) {
  float probability = shared_vad_->Process(chunk);
  
  if (probability < 0.0f) {
    std::cerr << "[VadTurnPipeline] VAD processing failed for session: " << session_id_ << "\n";
    return;
  }

  VadState vad_state = shared_vad_->GetState();

  // State machine transitions
  if (current_state_ == VadState::kIdle && vad_state == VadState::kSpeech) {
    HandleSpeechDetected(probability);
  } else if ((current_state_ == VadState::kSpeech || current_state_ == VadState::kSilence) 
             && vad_state == VadState::kIdle) {
    HandleSilenceDetected();
  } else if (current_state_ == VadState::kSilence && vad_state == VadState::kSpeech) {
    // User resumed speaking during silence - back to speech state
    current_state_ = VadState::kSpeech;
    std::cout << "[VadTurnPipeline] Speech resumed during silence for session: " 
              << session_id_ << "\n";
  }
  
  // Update current state to match VAD state
  if (vad_state == VadState::kSilence && current_state_ == VadState::kSpeech) {
    current_state_ = VadState::kSilence;
  }
}

void VadTurnPipeline::HandleSpeechDetected(float probability) {
  // Only on fresh turn, not when resuming mid-utterance
  if (current_state_ == VadState::kIdle) {
    turn_buffer_->Clear();
    speech_start_time_ = std::chrono::steady_clock::now();
  }

  current_state_ = VadState::kSpeech;
  
  std::cout << "[VadTurnPipeline] Speech started (probability: " << probability 
            << ") for session: " << session_id_ << "\n";
  
  // Emit speech_started event
  if (session_ && session_->streams && session_->streams->GetEventStream()) {
    session_->streams->GetEventStream()->SendOnSpeechStartedEvent(GetCurrentTimestampMs());
  } else {
    std::cerr << "[VadTurnPipeline] Event stream not available for session: " 
              << session_id_ << "\n";
  }
}

void VadTurnPipeline::HandleSilenceDetected() {
  if (current_state_ == VadState::kSpeech) {
    silence_start_time_ = std::chrono::steady_clock::now();
  
    // Backchannel filter
    auto speech_duration = std::chrono::steady_clock::now() - speech_start_time_;
    auto speech_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(speech_duration).count();
    
    if (speech_duration_ms < kMinUtteranceDurationMs) {
      std::cout << "[VadTurnPipeline] Short utterance (" << speech_duration_ms
                << "ms), treating as backchannel for session: " << session_id_ << "\n";
      current_state_ = VadState::kIdle;
      turn_buffer_->Clear();
      return;
    }
    
    // Emit speech_ended event only on first silence
    if (session_ && session_->streams && session_->streams->GetEventStream()) {
      session_->streams->GetEventStream()->SendOnSpeechEndedEvent(GetCurrentTimestampMs());
    }
  }

  current_state_ = VadState::kSilence;
  
  // Trigger SmartTurn detection
  TriggerSmartTurnDetection();
}

void VadTurnPipeline::TriggerSmartTurnDetection() {
  if (turn_buffer_->IsEmpty()) {
    std::cout << "[VadTurnPipeline] Turn buffer empty, skipping SmartTurn for session: " 
              << session_id_ << "\n";
    current_state_ = VadState::kIdle;
    return;
  }

  const std::vector<float>& audio_buffer = turn_buffer_->GetBuffer();
  float turn_probability = shared_turn_detector_->Process(audio_buffer);
  
  if (turn_probability < 0.0f) {
    std::cerr << "[VadTurnPipeline] SmartTurn processing failed for session: " 
              << session_id_ << "\n";
    current_state_ = VadState::kIdle;
    return;
  }

  bool is_turn_complete = shared_turn_detector_->IsTurnComplete(turn_probability);

  // Dynamic timeout based on SmartTurn confidence
  uint32_t timeout_ms;
  if (turn_probability < 0.2f) {
    timeout_ms = kSilenceTimeoutExtendedMs; // Likely mid-sentence
  } else if (turn_probability < 0.5f) {
    // Linear interpolation: 0.2 → 2000ms, 0.5 → 1000ms
    timeout_ms = kSilenceTimeoutExtendedMs - 
        static_cast<uint32_t>((turn_probability - 0.2f) / 0.3f * 
        (kSilenceTimeoutExtendedMs - kSilenceTimeoutBaseMs));
  } else {
    timeout_ms = kSilenceTimeoutBaseMs;
  }

  // Check if silence timeout has been reached
  auto silence_duration = std::chrono::steady_clock::now() - silence_start_time_;
  auto silence_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(silence_duration).count();
  bool timeout_reached = silence_duration_ms >= timeout_ms;

  if (is_turn_complete || timeout_reached) {
    if (timeout_reached && !is_turn_complete) {
      std::cout << "[VadTurnPipeline] Turn timeout after " << silence_duration_ms 
                << "ms for session: " << session_id_ << "\n";
    } else {
      std::cout << "[VadTurnPipeline] Turn complete (confidence: " << turn_probability 
                << ") for session: " << session_id_ << "\n";
    }
    
    // Emit user_turn_complete event
    if (session_ && session_->streams && session_->streams->GetEventStream()) {
      session_->streams->GetEventStream()->SendOnUserTurnCompleteEvent(
          turn_probability, GetCurrentTimestampMs());
    }
    
    // Clear buffer and return to idle
    turn_buffer_->Clear();
    current_state_ = VadState::kIdle;
  } else {
    std::cout << "[VadTurnPipeline] Turn incomplete (confidence: " << turn_probability 
              << "), waiting for session: " << session_id_ << "\n";
  }
}

uint64_t VadTurnPipeline::GetCurrentTimestampMs() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

}  // namespace saasy::listening_engine
