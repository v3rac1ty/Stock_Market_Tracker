#!/bin/bash
set -e

echo "Building Stock Market Tracker..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

echo "Build completed successfully"
echo "Executable location: $(pwd)/bin/stock_market_tracker"
echo ""
echo "To run the application:"
echo "$(pwd)/bin/stock_market_tracker"

cd ..
