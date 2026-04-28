cmake_minimum_required(VERSION 3.22)

project(basicFMPolysynth VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point this at your JUCE checkout or install location.
# Example:
#   cmake -S . -B build -DJUCE_DIR=/Users/you/dev/JUCE
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE path, e.g. -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(basicFMPolysynth
    COMPANY_NAME "GRAME"
    BUNDLE_ID "com.grame.basicFMPolysynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE 11bd
    FORMATS VST3
    PRODUCT_NAME "basicFMPolysynth"
)

juce_generate_juce_header(basicFMPolysynth)

target_sources(basicFMPolysynth
    PRIVATE
        basicFMPolysynth/FaustPluginProcessor.cpp
)

target_compile_definitions(basicFMPolysynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        MIDICTRL=1
        NVOICES=NUM_VOICES
        FAUSTFLOAT=float
)

target_include_directories(basicFMPolysynth
    PRIVATE
        /usr/local/include
        /opt/homebrew/include
)

target_link_libraries(basicFMPolysynth
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
