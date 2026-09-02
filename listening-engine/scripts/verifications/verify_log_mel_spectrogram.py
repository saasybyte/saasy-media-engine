"""
Verification script to compare C++ LogMelSpectrogram output against Hugging Face.
"""

import sys
import numpy as np
from transformers import WhisperFeatureExtractor

# Configuration (Must match C++ Test Harness)
CPP_OUTPUT_FILE = "cpp_log_mel_spectrogram_output.bin"
SAMPLE_RATE = 16000
DURATION_SEC = 8.0
N_MELS = 80
EXPECTED_FRAMES = 800  # 8 seconds * 100 frames/sec
SINE_FREQ = 440.0

def compare_results():
    """Compare C++ mel spectrogram output against Hugging Face reference implementation."""

    print("--- Starting Verification ---")

    # Load C++ Output
    try:
        cpp_data = np.fromfile(CPP_OUTPUT_FILE, dtype=np.float32)
    except FileNotFoundError:
        print(f"❌ Error: {CPP_OUTPUT_FILE} not found. Run the C++ test first.")
        sys.exit(1)

    # Validate Shape
    # C++ output is flattened [80, 800]
    expected_floats = N_MELS * EXPECTED_FRAMES
    if cpp_data.size != expected_floats:
        print(f"❌ Shape Mismatch. C++ produced {cpp_data.size} floats.")
        print(f"   Expected {N_MELS} * {EXPECTED_FRAMES} = {expected_floats} floats.")
        sys.exit(1)

    cpp_mel = cpp_data.reshape(N_MELS, EXPECTED_FRAMES)
    print(f"C++ Shape: {cpp_mel.shape} ({N_MELS} filters, {EXPECTED_FRAMES} frames)")

    # Generate Exact Same Audio
    # Using 64-bit float generation to match C++ test logic and prevent phase drift
    t = np.linspace(0, DURATION_SEC, int(SAMPLE_RATE * DURATION_SEC), endpoint=False)
    audio = np.sin(2 * np.pi * SINE_FREQ * t).astype(np.float32)

    # Run Hugging Face (The "Gold Standard")
    print("Running Hugging Face WhisperFeatureExtractor...")
    extractor = WhisperFeatureExtractor.from_pretrained("openai/whisper-tiny")

    # HF pads to 30s (3000 frames) by default
    hf_features = extractor(audio, sampling_rate=SAMPLE_RATE, return_tensors="pt").input_features[0].numpy()
    print(f"HF Shape:  {hf_features.shape} (Standard 30s padding)")

    # Slice and Compare
    # We only compare the frames relevant to our 8s window
    hf_sliced = hf_features[:, :EXPECTED_FRAMES]

    diff = np.abs(cpp_mel - hf_sliced)
    mean_diff = np.mean(diff)
    max_diff = np.max(diff)

    print("-" * 30)
    print(f"Mean Difference: {mean_diff:.6f}")
    print(f"Max Difference:  {max_diff:.6f}")

    # Verdict
    # We expect near-zero difference (< 1e-3)
    if mean_diff < 1e-3:
        print("✅ SUCCESS: C++ implementation matches Hugging Face.")
        print("   Your ONNX pipeline is ready to be built.")
        sys.exit(0)
    else:
        print("❌ FAILURE: Significant deviation detected.")
        print("   Check sample rate, padding logic, or byte order.")
        sys.exit(1)

if __name__ == "__main__":
    compare_results()
