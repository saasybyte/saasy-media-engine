#include "vad_turn/smart_turn_detector.h"

#include <iostream>

#include "vad_turn/log_mel_spectrogram.h"

namespace saasy::listening_engine {

SmartTurnDetector::SmartTurnDetector(const std::string& model_path, 
                                     const TurnDetectorConfig& config)
    : config_(config),
      env_(ORT_LOGGING_LEVEL_WARNING, "SmartTurnDetector"),
      session_(nullptr),
      memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {

  InitializeOnnxSession(model_path);
  
  std::cout << "[SmartTurnDetector] Created (threshold: " << config_.turn_threshold 
            << ", threads for mel spectrogram: " << config_.n_threads_for_mel << ")\n";
}

SmartTurnDetector::~SmartTurnDetector() {
  std::cout << "[SmartTurnDetector] Destroyed\n";
}

void SmartTurnDetector::InitializeOnnxSession(const std::string& model_path) {
  Ort::SessionOptions session_options;
  session_options.SetIntraOpNumThreads(1);
  session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

  try {
    session_ = Ort::Session(env_, model_path.c_str(), session_options);
    std::cout << "[SmartTurnDetector] ONNX model loaded: " << model_path << "\n";
  } catch (const Ort::Exception& e) {
    std::cerr << "[SmartTurnDetector] Failed to load model: " << e.what() << "\n";
    throw;
  }
}

float SmartTurnDetector::Process(const std::vector<float>& audio_buffer) {
  if (audio_buffer.empty()) {
    std::cerr << "[SmartTurnDetector] Empty audio buffer\n";
    return -1.0f;
  }

  try {
    // Truncate to most recent 8 seconds if needed
    const float* audio_data = audio_buffer.data();
    int n_samples = static_cast<int>(audio_buffer.size());
    
    if (n_samples > static_cast<int>(kMaxSamples)) {
      // Keep most recent 8 seconds by advancing pointer to skip oldest samples
      // Example: 200k samples → pointer moves to index 72k, reads last 128k
      audio_data = audio_buffer.data() + (n_samples - kMaxSamples);
      n_samples = static_cast<int>(kMaxSamples);
      std::cout << "[SmartTurnDetector] Truncated audio to 8 seconds\n";
    }

    // Convert audio to mel spectrogram
    LogMelSpectrogram::Output mel_output;
    if (!LogMelSpectrogram::Compute(audio_data, n_samples, config_.n_threads_for_mel, mel_output)) {
      std::cerr << "[SmartTurnDetector] Failed to compute mel spectrogram\n";
      return -1.0f;
    }

    // Prepare input tensor
    // Expected shape: [1, 80, n_frames] (batch, mel_bins, time)
    std::vector<int64_t> input_shape = {
      1, 
      static_cast<int64_t>(mel_output.n_mel),
      static_cast<int64_t>(mel_output.n_len)
    };

    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_, 
        mel_output.data.data(), 
        mel_output.data.size(),
        input_shape.data(), 
        input_shape.size());

    // Run inference
    const char* input_names[] = {"input_features"};
    const char* output_names[] = {"logits"}; // Despite the name, output is sigmoid-activated probability [0.0, 1.0]
    
    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(std::move(input_tensor));

    auto output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        input_names, 
        input_tensors.data(), 
        input_tensors.size(),
        output_names, 
        1);

    // Extract turn completion probability
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    float probability = output_data[0];

    std::cout << "[SmartTurnDetector] Turn probability: " << probability 
              << " (audio duration: " << (n_samples * 1000 / kSampleRate) << "ms)\n";

    return probability;

  } catch (const Ort::Exception& e) {
    std::cerr << "[SmartTurnDetector] Inference failed: " << e.what() << "\n";
    return -1.0f;
  }
}

}  // namespace saasy::listening_engine
