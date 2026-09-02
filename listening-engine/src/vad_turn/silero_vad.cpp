#include "vad_turn/silero_vad.h"

#include <iostream>

namespace saasy::listening_engine {

SileroVAD::SileroVAD(const std::string& model_path, const VadConfig& config)
    : config_(config),
      state_(VadState::kIdle),
      speech_duration_ms_(0),
      silence_duration_ms_(0),
      env_(ORT_LOGGING_LEVEL_WARNING, "SileroVAD"),
      session_(nullptr),
      memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      sample_rate_tensor_(kSampleRate) {

  // Initialize state tensor
  state_tensor_.resize(2 * 1 * kStateDim, 0.0f); // [2, 1, 128] = 256 elements
  context_buffer_.resize(kContextSamples, 0.0f); // 64 samples

  InitializeOnnxSession(model_path);
  
  std::cout << "[SileroVAD] Created (threshold: " << config_.threshold 
            << ", min_speech: " << config_.min_speech_duration_ms << "ms"
            << ", min_silence: " << config_.min_silence_duration_ms << "ms)\n";
}

SileroVAD::~SileroVAD() {
  std::cout << "[SileroVAD] Destroyed\n";
}

void SileroVAD::InitializeOnnxSession(const std::string& model_path) {
  Ort::SessionOptions session_options;
  session_options.SetIntraOpNumThreads(1);
  session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

  try {
    session_ = Ort::Session(env_, model_path.c_str(), session_options);
    std::cout << "[SileroVAD] ONNX model loaded: " << model_path << "\n";
  } catch (const Ort::Exception& e) {
    std::cerr << "[SileroVAD] Failed to load model: " << e.what() << "\n";
    throw;
  }
}

float SileroVAD::Process(const std::vector<float>& audio_chunk) {
  if (audio_chunk.size() != kChunkSize) {
    std::cerr << "[SileroVAD] Invalid chunk size: " << audio_chunk.size() 
              << " (expected " << kChunkSize << ")\n";
    return -1.0f;
  }

  try {
    // Build effective input: context (64) + audio chunk (512) = 576 samples
    std::vector<float> effective_input(kEffectiveInputSize, 0.0f);
    std::copy(context_buffer_.begin(), context_buffer_.end(), effective_input.begin());
    std::copy(audio_chunk.begin(), audio_chunk.end(), effective_input.begin() + kContextSamples);

    // Prepare input tensors
    std::vector<int64_t> audio_shape = {1, static_cast<int64_t>(kEffectiveInputSize)};
    std::vector<int64_t> state_shape = {2, 1, static_cast<int64_t>(kStateDim)};
    std::vector<int64_t> sample_rate_shape = {1};

    auto audio_tensor = Ort::Value::CreateTensor<float>(
        memory_info_, effective_input.data(), kEffectiveInputSize,
        audio_shape.data(), audio_shape.size());
    
    auto state_tensor = Ort::Value::CreateTensor<float>(
        memory_info_, state_tensor_.data(), state_tensor_.size(),
        state_shape.data(), state_shape.size());
    
    auto sample_rate_tensor = Ort::Value::CreateTensor<int64_t>(
        memory_info_, &sample_rate_tensor_, 1, sample_rate_shape.data(), sample_rate_shape.size());

    // Run inference (v6: 3 inputs, 2 outputs)
    const char* input_names[] = {"input", "state", "sr"};
    const char* output_names[] = {"output", "stateN"};
    
    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(std::move(audio_tensor));
    input_tensors.push_back(std::move(state_tensor));
    input_tensors.push_back(std::move(sample_rate_tensor));

    auto output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        input_names, input_tensors.data(), input_tensors.size(),
        output_names, 2);

    // Extract speech probability
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    float probability = output_data[0];

    // Update state tensor for next iteration
    float* new_state = output_tensors[1].GetTensorMutableData<float>();
    std::copy(new_state, new_state + state_tensor_.size(), state_tensor_.begin());

    // Update context buffer: save last 64 samples for next iteration
    std::copy(effective_input.end() - kContextSamples, effective_input.end(), context_buffer_.begin());

    // Update VAD state machine
    UpdateState(probability);

    return probability;

  } catch (const Ort::Exception& e) {
    std::cerr << "[SileroVAD] Inference failed: " << e.what() << "\n";
    return -1.0f;
  }
}

void SileroVAD::UpdateState(float probability) {
  if (probability >= config_.threshold) {
    // Speech detected
    speech_duration_ms_ += kChunkDurationMs;
    silence_duration_ms_ = 0;

    if (state_ == VadState::kIdle && speech_duration_ms_ >= config_.min_speech_duration_ms) {
      state_ = VadState::kSpeech;
      std::cout << "[SileroVAD] Speech started (probability: " << probability << ")\n";
    } else if (state_ == VadState::kSilence) {
      state_ = VadState::kSpeech;
    }
  } else {
    // Silence detected
    silence_duration_ms_ += kChunkDurationMs;
    
    if (state_ == VadState::kSpeech) {
      state_ = VadState::kSilence;
    }

    if (state_ == VadState::kSilence && silence_duration_ms_ >= config_.min_silence_duration_ms) {
      state_ = VadState::kIdle;
      speech_duration_ms_ = 0;
      std::cout << "[SileroVAD] Speech ended (silence duration: " << silence_duration_ms_ << "ms)\n";
    }
  }
}

void SileroVAD::Reset() {
  std::fill(state_tensor_.begin(), state_tensor_.end(), 0.0f);
  std::fill(context_buffer_.begin(), context_buffer_.end(), 0.0f);
  state_ = VadState::kIdle;
  speech_duration_ms_ = 0;
  silence_duration_ms_ = 0;
  std::cout << "[SileroVAD] State reset\n";
}

}  // namespace saasy::listening_engine
