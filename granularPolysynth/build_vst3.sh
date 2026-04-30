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
JUCE_PLUGIN_ARCH="$SCRIPT_DIR/juce/arch/juce-plugin-soundbrowse.cpp"
if [[ ! -f "$JUCE_PLUGIN_ARCH" ]]; then
    echo "Missing plugin architecture: $JUCE_PLUGIN_ARCH" >&2
    exit 1
fi

OUT_CPP="$SCRIPT_DIR/granularPolysynth/FaustPluginProcessor.cpp"
OUT_EFFECT="$SCRIPT_DIR/granularPolysynth/effect.h"
mkdir -p "$SCRIPT_DIR/granularPolysynth"

echo "Generating Faust POLY2 post-mix effect"
faust -i --import-dir "$SCRIPT_DIR/.." \
    -scn base_dsp \
    -cn effect \
    -o "$OUT_EFFECT" \
    "$SCRIPT_DIR/effect.dsp"

echo "Generating Faust → JUCE plugin sources"
faust -scn base_dsp -uim -i --import-dir "$SCRIPT_DIR/.." \
    -a "$JUCE_PLUGIN_ARCH" \
    -o "$OUT_CPP" \
    "$SCRIPT_DIR/granularPolysynth.dsp"

perl -i -pe 's/#include\s+"JuceLibraryCode\/JuceHeader\.h"/#include <JuceHeader.h>/g' "$OUT_CPP"

echo "Configuring granularPolysynth (VST3)"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER" \
    -DJUCE_DIR="$JUCE_DIR"

echo "Building VST3 ($CONFIG)"
cmake --build "$BUILD_DIR" --config "$CONFIG"

VST3_BUILD_PATH="$BUILD_DIR/granularPolysynth_artefacts/$CONFIG/VST3/granularPolysynth.vst3"
VST3_INSTALL_DIR="/Users/charlesabbott/Library/Audio/Plug-Ins/VST3"
VST3_INSTALL_PATH="$VST3_INSTALL_DIR/granularPolysynth.vst3"

echo "Installing VST3 to user plug-in folder"
mkdir -p "$VST3_INSTALL_DIR"
rm -rf "$VST3_INSTALL_PATH"
cp -R "$VST3_BUILD_PATH" "$VST3_INSTALL_PATH"

echo
echo "Built plugin:"
echo "  $VST3_BUILD_PATH"
echo "Installed plugin:"
echo "  $VST3_INSTALL_PATH"
