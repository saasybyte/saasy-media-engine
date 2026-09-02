#include "vad_turn/log_mel_spectrogram.h"

#include <iostream>
#include <vector>
#include <fstream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <chrono>

using saasy::listening_engine::LogMelSpectrogram;

int main() {
    const int kSampleRate = 16000;
    const char* kFilterPath = "listening-engine/assets/mel_80.bin";
    const char* kOutputPath = "cpp_log_mel_spectrogram_output.bin";

    // Initialize the System
    std::cout << "Initializing LogMelSpectrogram..." << std::endl;
    if (!LogMelSpectrogram::Init(kFilterPath)) {
        std::cerr << "Failed to initialize LogMelSpectrogram. Check path: " << kFilterPath << std::endl;
        return 1;
    }

    // Generate 8 Seconds of Audio (Sine Wave 440Hz)
    // Using double precision for generation to prevent phase drift (matches verify.py)
    int n_input_samples = kSampleRate * 8; 
    std::vector<float> audio(n_input_samples);
    
    for(int i = 0; i < n_input_samples; ++i) {
        double t = static_cast<double>(i) / kSampleRate;
        audio[i] = static_cast<float>(std::sin(2.0 * M_PI * 440.0 * t));
    }

    // Prepare Output Container
    LogMelSpectrogram::Output output;

    // Run the Algorithm
    std::cout << "Computing Spectrogram..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    bool success = LogMelSpectrogram::Compute(
        audio.data(), 
        audio.size(), 
        1, // n_threads
        output
    );

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Execution Time: " << duration_ms << " ms" << std::endl;

    if (!success) {
        std::cerr << "Failed to compute spectrogram." << std::endl;
        return 1;
    }

    // Dump to file for Python verification
    std::ofstream out(kOutputPath, std::ios::binary);
    out.write(reinterpret_cast<char*>(output.data.data()), output.data.size() * sizeof(float));
    out.close();

    std::cout << "Output saved to " << kOutputPath << std::endl;
    std::cout << "Spectrogram Size: " << output.n_mel << " x " << output.n_len << std::endl;
    
    // Expectation: 8s audio -> ~800 frames
    if (output.n_len == 800) {
        std::cout << "✅ Frame count matches 8s target (800 frames)." << std::endl;
    } else {
        std::cout << "⚠️ Frame count is " << output.n_len << " (Expected 800)." << std::endl;
    }

    return 0;
}
