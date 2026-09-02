"""Binary mel filter bank inspection utility."""

import sys

import numpy as np

def check_bin():
    """
    Verify mel filter bank binary file format and content.
    
    Expects 80x201 float32 mel filters. Takes filename as arg or uses default.
    """

    # Get filename from args or default to hardcoded path
    if len(sys.argv) > 1:
        filename = sys.argv[1]
    else:
        filename = "listening-engine/assets/mel_80.bin"

    print(f"🔍 Inspecting: {filename}")

    # Load Data
    try:
        data = np.fromfile(filename, dtype=np.float32)
    except FileNotFoundError:
        print(f"❌ Error: File {filename} not found.")
        sys.exit(1)

    # Check Size
    # Expected: 80 filters * 201 bins = 16080 floats
    expected_size = 80 * 201
    if data.size == expected_size:
        print(f"✅ Size Correct: {data.size} floats (80x201).")
    else:
        print(f"❌ Size Mismatch: Found {data.size}, expected {expected_size}.")
        print("   (Check if file has a header or is float64)")
        sys.exit(1)

    # Check Values (Sanity)
    # Mel filters are mostly 0, max value usually < 1.0
    if data.max() > 10.0 or np.isnan(data).any():
        print("❌ Values look suspicious (too high or NaN). Check Endianness.")
        sys.exit(1)
    else:
        print(f"✅ Values look reasonable (Max: {data.max():.4f})")

if __name__ == "__main__":
    check_bin()
