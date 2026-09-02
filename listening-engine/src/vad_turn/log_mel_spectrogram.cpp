#include "vad_turn/log_mel_spectrogram.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#define _USE_MATH_DEFINES
#include <cmath>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

namespace saasy::listening_engine {

// Static member definitions
float LogMelSpectrogram::sin_vals_[kSinCosTableSize];
float LogMelSpectrogram::cos_vals_[kSinCosTableSize];
float LogMelSpectrogram::hann_window_[kFFTSize];
LogMelSpectrogram::Filters LogMelSpectrogram::filters_;
bool LogMelSpectrogram::initialized_ = false;

bool LogMelSpectrogram::Init(const char* filter_filename) {
  if (initialized_) {
    std::cout << "[LogMelSpectrogram] Already initialized\n";
    return true;
  }

  FillSinCosTable();
  FillHannWindow();
  
  if (!LoadFilters(filter_filename)) {
    return false;
  }

  initialized_ = true;
  std::cout << "[LogMelSpectrogram] Initialized successfully\n";
  return true;
}

void LogMelSpectrogram::FillSinCosTable() {
  for (int i = 0; i < kSinCosTableSize; i++) {
    double theta = (2.0 * M_PI * i) / kSinCosTableSize;
    sin_vals_[i] = sinf(theta);
    cos_vals_[i] = cosf(theta);
  }
}

void LogMelSpectrogram::FillHannWindow() {
  int length = sizeof(hann_window_)/sizeof(hann_window_[0]);

  for (int i = 0; i < length; i++) {
    hann_window_[i] = 0.5f * (1.0f - cosf((2.0 * M_PI * i) / length));
  }
}

bool LogMelSpectrogram::LoadFilters(const char* filename) {
  filters_.n_fft = kFFTSize / 2 + 1;
  filters_.n_mel = kNumMelBins;
  filters_.data.resize(filters_.n_mel * filters_.n_fft);

  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "[LogMelSpectrogram] Failed to open filter file: " << filename << "\n";
    return false;
  }

  file.read(reinterpret_cast<char*>(filters_.data.data()), filters_.data.size() * sizeof(float));
  
  if (!file) {
    std::cerr << "[LogMelSpectrogram] Failed to read filter data from: " << filename << "\n";
    return false;
  }

  std::cout << "[LogMelSpectrogram] Loaded " << filters_.n_mel << "x" << filters_.n_fft 
            << " mel filter bank from " << filename << "\n";

  file.close();
  return true;
}

// Naive Discrete Fourier Transform (fallback for non-power-of-2 sizes)
void LogMelSpectrogram::DFT(const float* in, int N, float* out) {
  const int sin_cos_step = kSinCosTableSize / N;

  for (int k = 0; k < N; k++) {
    float re = 0.0f;
    float im = 0.0f;

    for (int n = 0; n < N; n++) {
      int idx = (k * n * sin_cos_step) % kSinCosTableSize;
      re += in[n] * cos_vals_[idx];
      im -= in[n] * sin_vals_[idx];
    }

    out[k * 2 + 0] = re;
    out[k * 2 + 1] = im;
  }
}

// Cooley-Tukey FFT (optimized for power-of-2 sizes)
void LogMelSpectrogram::FFT(float* in, int N, float* out) {
  if (N == 1) {
    out[0] = in[0];
    out[1] = 0.0f;
    return;
  }

  const int half_N = N / 2;
  if (N - half_N * 2 == 1) {
    // Not power of 2, fall back to DFT
    DFT(in, N, out);
    return;
  }

  // Split into even and odd
  float* even = in + N;
  for (int i = 0; i < half_N; ++i) {
    even[i] = in[2 * i];
  }
  float* even_fft = out + 2 * N;
  FFT(even, half_N, even_fft);

  float* odd = even;
  for (int i = 0; i < half_N; ++i) {
    odd[i] = in[2 * i + 1];
  }
  float* odd_fft = even_fft + N;
  FFT(odd, half_N, odd_fft);

  // Combine results
  const int sin_cos_step = kSinCosTableSize / N;
  for (int k = 0; k < half_N; ++k) {
    int idx = k * sin_cos_step;
    float re = cos_vals_[idx];
    float im = -sin_vals_[idx];

    float re_odd = odd_fft[2 * k + 0];
    float im_odd = odd_fft[2 * k + 1];

    out[2 * k + 0] = even_fft[2 * k + 0] + re * re_odd - im * im_odd;
    out[2 * k + 1] = even_fft[2 * k + 1] + re * im_odd + im * re_odd;

    out[2 * (k + half_N) + 0] = even_fft[2 * k + 0] - re * re_odd + im * im_odd;
    out[2 * (k + half_N) + 1] = even_fft[2 * k + 1] - re * im_odd - im * re_odd;
  }
}

void LogMelSpectrogram::WorkerThread(
    int thread_id,
    const float* hann_window,
    const std::vector<float>& samples,
    int n_samples,
    int frame_size,
    int frame_step,
    int n_threads,
    const Filters& filters,
    Output& output) {
  
  std::vector<float> fft_in(frame_size * 2, 0.0f);
  std::vector<float> fft_out(frame_size * 2 * 2 * 2);

  int n_fft = filters.n_fft;
  int i = thread_id;

  // Ensure n_fft matches expected size (1 + FFT_SIZE/2)
  assert(n_fft == 1 + (frame_size / 2));

  // Process frames assigned to this thread
  for (; i < std::min(n_samples / frame_step + 1, output.n_len); i += n_threads) {
    const int offset = i * frame_step;

    // Apply Hann window
    for (int j = 0; j < std::min(frame_size, n_samples - offset); j++) {
      fft_in[j] = hann_window[j] * samples[offset + j];
    }

    // Zero-pad if needed
    if (n_samples - offset < frame_size) {
      std::fill(fft_in.begin() + (n_samples - offset), fft_in.end(), 0.0f);
    }

    // Compute FFT
    FFT(fft_in.data(), frame_size, fft_out.data());

    // Calculate power spectrum (magnitude squared)
    for (int j = 0; j < n_fft; j++) {
      fft_out[j] = fft_out[2 * j + 0] * fft_out[2 * j + 0] + 
                   fft_out[2 * j + 1] * fft_out[2 * j + 1];
    }

    // Apply mel filter bank
    for (int j = 0; j < output.n_mel; j++) {
      double sum = 0.0;
      
      // Unrolled loop for performance
      int k = 0;
      for (k = 0; k < n_fft - 3; k += 4) {
        sum += fft_out[k + 0] * filters.data[j * n_fft + k + 0] +
               fft_out[k + 1] * filters.data[j * n_fft + k + 1] +
               fft_out[k + 2] * filters.data[j * n_fft + k + 2] +
               fft_out[k + 3] * filters.data[j * n_fft + k + 3];
      }
      
      // Handle remainder
      for (; k < n_fft; k++) {
        sum += fft_out[k] * filters.data[j * n_fft + k];
      }
      
      // Log scale with floor to prevent log(0)
      sum = std::log10(std::max(sum, 1e-10));
      output.data[j * output.n_len + i] = static_cast<float>(sum);
    }
  }

  // Fill remaining frames with silence (-10 in log scale)
  double silence_value = std::log10(1e-10);
  for (; i < output.n_len; i += n_threads) {
    for (int j = 0; j < output.n_mel; j++) {
      output.data[j * output.n_len + i] = static_cast<float>(silence_value);
    }
  }
}

bool LogMelSpectrogram::Compute(
    const float* samples,
    int n_samples,
    int n_threads,
    Output& output) {
  
  if (!initialized_) {
    std::cerr << "[LogMelSpectrogram] Not initialized. Call Init() first.\n";
    return false;
  }

  if (!samples || n_samples <= 0 || n_threads <= 0) {
    return false;
  }

  const float* hann_window = hann_window_;

  // Calculate padding to reach target duration (8 seconds)
  int64_t stage_1_pad = std::max(static_cast<int64_t>(0), 
                                 static_cast<int64_t>(kTargetSampleCount) - n_samples);
  int64_t stage_2_pad = kFFTSize / 2;

  // Prepare padded sample buffer
  std::vector<float> samples_padded;
  samples_padded.resize(n_samples + stage_1_pad + stage_2_pad * 2);
  std::copy(samples, samples + n_samples, samples_padded.begin() + stage_2_pad);

  // Zero-pad to target duration
  std::fill(samples_padded.begin() + n_samples + stage_2_pad,
            samples_padded.begin() + n_samples + stage_1_pad + 2 * stage_2_pad,
            0.0f);

  // Reflective padding at start (200 samples)
  std::reverse_copy(samples + 1, samples + 1 + stage_2_pad, samples_padded.begin());

  // Setup output structure
  output.n_mel = kNumMelBins;
  output.n_len = static_cast<int>((samples_padded.size() - kFFTSize) / kHopLength);
  output.n_len_org = 1 + (n_samples + stage_2_pad - kFFTSize) / kHopLength;
  output.data.resize(output.n_mel * output.n_len);

  // Multi-threaded mel spectrogram computation
  {
    std::vector<std::thread> workers(n_threads - 1);
    for (int iw = 0; iw < n_threads - 1; ++iw) {
      workers[iw] = std::thread(
          WorkerThread, iw + 1, hann_window, std::cref(samples_padded),
          n_samples + stage_2_pad, kFFTSize, kHopLength, n_threads,
          std::cref(filters_), std::ref(output));
    }

    // Main thread participates
    WorkerThread(0, hann_window, samples_padded, n_samples + stage_2_pad,
                kFFTSize, kHopLength, n_threads, filters_, output);

    // Wait for all threads
    for (int iw = 0; iw < n_threads - 1; ++iw) {
      workers[iw].join();
    }
  }

  // Clamping and normalization
  double max_value = -1e20;
  for (int i = 0; i < output.n_mel * output.n_len; i++) {
    if (output.data[i] > max_value) {
      max_value = output.data[i];
    }
  }

  max_value -= 8.0;

  for (int i = 0; i < output.n_mel * output.n_len; i++) {
    if (output.data[i] < max_value) {
      output.data[i] = static_cast<float>(max_value);
    }
    output.data[i] = static_cast<float>((output.data[i] + 4.0) / 4.0);
  }

  return true;
}

}  // namespace saasy::listening_engine
