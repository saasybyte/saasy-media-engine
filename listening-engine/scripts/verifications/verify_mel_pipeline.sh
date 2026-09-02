#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# --- Configuration ---
INCLUDE_DIR="listening-engine/include"
SRC_DIR="listening-engine/src"
SCRIPT_DIR="listening-engine/scripts/verifications"
ASSET_FILE="listening-engine/assets/mel_80.bin"
OUTPUT_BIN="run_cpp_log_mel_spectrogram_verification"
DATA_BIN="cpp_log_mel_spectrogram_output.bin"

echo "=========================================="
echo "   Log Mel Spectrogram Verification "
echo "=========================================="

# Pre-flight Check
if [ ! -f "$ASSET_FILE" ]; then
    echo "❌ Error: Asset file '$ASSET_FILE' not found."
    echo "   Please ensure the mel_80.bin file is in the assets directory."
    exit 1
fi

# Pre-flight Check: Binary Content Verification
echo "------------------------------------------"
echo "🔍 Verifying mel_80.bin integrity..."
python3 "$SCRIPT_DIR/verify_mel_80_bin.py" "$ASSET_FILE"

# Compile C++
echo "🔨 Compiling C++ Test Harness..."
g++ -std=c++17 -O3 \
    -I "$INCLUDE_DIR" \
    "$SCRIPT_DIR/verify_log_mel_spectrogram.cpp" \
    "$SRC_DIR/vad_turn/log_mel_spectrogram.cpp" \
    -o "$OUTPUT_BIN" \
    -lpthread -lm

echo "✅ Compilation Successful."

# Run C++ Binary
echo "------------------------------------------"
echo "Running C++ Implementation..."
./"$OUTPUT_BIN"

# Run Python Verification
echo "------------------------------------------"
echo "Running Python Validation..."
python3 "$SCRIPT_DIR/verify_log_mel_spectrogram.py"

# Cleanup
echo "------------------------------------------"
echo "Cleaning up temporary files..."
rm -f "$OUTPUT_BIN" "$DATA_BIN"

echo "=========================================="
echo "   ✅ Verification Pipeline Complete"
echo "=========================================="
