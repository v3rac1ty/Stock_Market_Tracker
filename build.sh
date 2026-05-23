#!/usr/bin/env bash
# Build script for StockMarketTracker CMake project (Linux/macOS)

set -e

# Default build type
BUILD_TYPE="Release"

# Accept optional first argument for build type
if [[ -n "$1" ]]; then
    BUILD_TYPE="$1"
fi

# Detect OS and get CPU count
if [[ "$OSTYPE" == "darwin"* ]]; then
    NUM_JOBS=$(sysctl -n hw.ncpu)
else
    NUM_JOBS=$(nproc)
fi

echo "[*] Building StockMarketTracker with CMake"
echo "[*] Build type: $BUILD_TYPE"
echo "[*] Parallel jobs: $NUM_JOBS"

# Configure
echo "[*] Configuring CMake..."
cmake -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

# Build
echo "[*] Building..."
cmake --build build -- -j"$NUM_JOBS"

echo "[OK] Build succeeded"
echo "[*] Output directory: build/"
