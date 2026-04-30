cmake_minimum_required(VERSION 3.22)

project(granularPolysynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point this at your JUCE checkout or install location.
# Example:
#   cmake -S . -B build -DJUCE_DIR=/Users/you/dev/JUCE
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE path, e.g. -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(granularPolysynth
    COMPANY_NAME "GRAME"
    BUNDLE_ID "com.grame.granularPolysynth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Gnps
    FORMATS VST3
    PRODUCT_NAME "granularPolysynth"
)

juce_generate_juce_header(granularPolysynth)

target_sources(granularPolysynth
    PRIVATE
        granularPolysynth/FaustPluginProcessor.cpp
)

target_compile_definitions(granularPolysynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        MIDICTRL=1
        NVOICES=NUM_VOICES
        NUM_VOICES=8
        SOUNDFILE=1
        FAUSTFLOAT=float
)

target_include_directories(granularPolysynth
    PRIVATE
        /usr/local/include
        /opt/homebrew/include
)

target_link_libraries(granularPolysynth
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(APPLE AND TARGET granularPolysynth_VST3)
    add_custom_command(
        TARGET granularPolysynth_VST3
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_BUNDLE_DIR:granularPolysynth_VST3>/Contents/Resources"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/placeholder.wav"
            "$<TARGET_BUNDLE_DIR:granularPolysynth_VST3>/Contents/Resources/placeholder.wav"
        COMMENT "Copy default sample into VST3 bundle Resources"
    )
endif()
