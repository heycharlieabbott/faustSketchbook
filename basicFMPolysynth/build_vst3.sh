#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"
CONFIG="${CONFIG:-Release}"
JUCE_DIR="${JUCE_DIR:-/Users/charlesabbott/Desktop/Code/JUCE}"
CXX_COMPILER="${CXX_COMPILER:-$(xcrun --find clang++)}"

echo "Configuring basicFMPolysynth"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
  -DJUCE_DIR="$JUCE_DIR"

echo "Building VST3 ($CONFIG)"
cmake --build "$BUILD_DIR" --config "$CONFIG"

echo
echo "Built plugin:"
echo "  $BUILD_DIR/basicFMPolysynth_artefacts/$CONFIG/VST3/basicFMPolysynth.vst3"
