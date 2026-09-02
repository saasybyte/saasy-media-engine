#pragma once

#include <cstdint>
#include <vector>

namespace saasy::listening_engine {

/**
 * LogMelSpectrogram converts audio to mel-scale spectrograms for SmartTurn v3.
 * 
 * Based on Whisper C++ implementation with optimized FFT and mel-filter bank.
 * Input audio is padded to 8 seconds (128,000 samples @ 16kHz) as required by SmartTurn.
 * 
 * Input:  16kHz, mono, 32-bit float PCM normalized to [-1.0, 1.0]
 * Output: 80-channel mel spectrogram with log scaling
 * 
 * Thread-safe: Uses multi-threaded FFT computation.
 * Must call Init() once before using Compute().
 */
class LogMelSpectrogram {
 public:
  struct Filters {
    int32_t n_mel;
    int32_t n_fft;
    std::vector<float> data;
  };

  struct Output {
    int n_len;      // Total frame count after padding
    int n_len_org;  // Original frame count (before full padding)
    int n_mel;      // Number of mel channels (80)
    std::vector<float> data;  // Row-major: [n_mel x n_len]
  };

  /**
   * Initialize mel spectrogram system.
   * Loads filter bank and precomputes lookup tables (sin/cos, Hann window).
   * Must be called once before Compute().
   * 
   * @param filter_filename Path to mel-filters binary file
   * @return true on success, false on failure
   */
  static bool Init(const char* filter_filename);

  /**
   * Compute log mel spectrogram from audio samples.
   * 
   * Pads input to 8 seconds (128,000 samples) with reflective padding at start.
   * Uses Hann windowing and FFT with mel-filter bank application.
   * 
   * @param samples Input audio samples
   * @param n_samples Number of input samples
   * @param n_threads Number of threads for parallel FFT computation
   * @param output Output mel spectrogram
   * @return true on success, false on failure
   */
  static bool Compute(
      const float* samples,
      int n_samples,
      int n_threads,
      Output& output);

 private:
  static constexpr int kSampleRate = 16000;
  static constexpr int kFFTSize = 400;
  static constexpr int kHopLength = 160;
  static constexpr int kTargetDurationSec = 8;
  static constexpr int kTargetSampleCount = kTargetDurationSec * kSampleRate;  // 128,000
  static constexpr int kSinCosTableSize = kFFTSize;
  static constexpr int kNumMelBins = 80;

  // Precomputed lookup tables
  static float sin_vals_[kSinCosTableSize];
  static float cos_vals_[kSinCosTableSize];
  static float hann_window_[kFFTSize];
  static Filters filters_;
  static bool initialized_;

  static void FillSinCosTable();

  static void FillHannWindow();

  static bool LoadFilters(const char* filename);

  static void FFT(float* in, int N, float* out);
  
  static void DFT(const float* in, int N, float* out);

  static void WorkerThread(
      int thread_id,
      const float* hann_window,
      const std::vector<float>& samples,
      int n_samples,
      int frame_size,
      int frame_step,
      int n_threads,
      const Filters& filters,
      Output& output);
};

}  // namespace saasy::listening_engine
