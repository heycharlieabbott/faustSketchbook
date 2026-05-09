cmake_minimum_required(VERSION 3.22)

# Universal macOS builds (arm64 + x86_64) so VST3 loads in both native and Rosetta DAWs.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS target architectures")
endif()

project(delayEffect1 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE path, e.g. -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(delayEffect1
    COMPANY_NAME "GRAME"
    BUNDLE_ID "com.grame.delayEffect1"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Dly1
    FORMATS VST3
    PRODUCT_NAME "delayEffect1"
)

juce_generate_juce_header(delayEffect1)

target_sources(delayEffect1
    PRIVATE
        FaustPluginProcessor.cpp
)

target_compile_definitions(delayEffect1
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        FAUSTFLOAT=float
)

target_include_directories(delayEffect1
    PRIVATE
        /usr/local/include
        /opt/homebrew/include
)

target_link_libraries(delayEffect1
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
