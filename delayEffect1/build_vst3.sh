#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"
CONFIG="${CONFIG:-Release}"
JUCE_DIR="${JUCE_DIR:-/Users/charlesabbott/Desktop/Code/JUCE}"
CXX_COMPILER="${CXX_COMPILER:-$(xcrun --find clang++ 2>/dev/null || echo clang++)}"

if command -v brew >/dev/null 2>&1; then
    FAUST_SHARE="$(brew --prefix faust)/share/faust"
else
    FAUST_SHARE="/usr/local/share/faust"
fi

JUCE_PLUGIN_ARCH="${JUCE_PLUGIN_ARCH:-$FAUST_SHARE/juce/juce-plugin.cpp}"
if [[ ! -f "$JUCE_PLUGIN_ARCH" ]]; then
    echo "Missing Faust JUCE architecture: $JUCE_PLUGIN_ARCH" >&2
    echo "Install Faust or set FAUST_SHARE / JUCE_PLUGIN_ARCH." >&2
    exit 1
fi

OUT_CPP="$SCRIPT_DIR/FaustPluginProcessor.cpp"
mkdir -p "$SCRIPT_DIR"

echo "Generating Faust → JUCE plugin sources (delayEffect1.dsp)"
faust -scn base_dsp -uim -i --import-dir "$SCRIPT_DIR/.." \
    -a "$JUCE_PLUGIN_ARCH" \
    -o "$OUT_CPP" \
    "$SCRIPT_DIR/delayEffect1.dsp"

perl -i -pe 's/#include\s+"JuceLibraryCode\/JuceHeader\.h"/#include <JuceHeader.h>/g' "$OUT_CPP"

echo "Configuring delayEffect1 (VST3)"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DJUCE_DIR="$JUCE_DIR"

echo "Building VST3 ($CONFIG)"
cmake --build "$BUILD_DIR" --config "$CONFIG"

VST3_BUILD_PATH="$BUILD_DIR/delayEffect1_artefacts/$CONFIG/VST3/delayEffect1.vst3"
VST3_INSTALL_DIR="/Users/charlesabbott/Library/Audio/Plug-Ins/VST3"
VST3_INSTALL_PATH="$VST3_INSTALL_DIR/delayEffect1.vst3"

echo "Installing VST3 to user plug-in folder"
mkdir -p "$VST3_INSTALL_DIR"
rm -rf "$VST3_INSTALL_PATH"
cp -R "$VST3_BUILD_PATH" "$VST3_INSTALL_PATH"

echo
echo "Built plugin:"
echo "  $VST3_BUILD_PATH"
echo "Installed plugin:"
echo "  $VST3_INSTALL_PATH"
