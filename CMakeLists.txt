cmake_minimum_required(VERSION 3.20)
project(SampleGrab VERSION 1.0.2)

# We use FetchContent to pull JUCE directly from GitHub during config
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(SampleGrab
    COMPANY_NAME "jerryrpt"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Jery
    PLUGIN_CODE Sgrp
    FORMATS VST3
    PRODUCT_NAME "SampleGrab"
)

juce_generate_juce_header(SampleGrab)

juce_add_binary_data(SampleGrabAssets
    SOURCES
        assets/Rectangle\ 1\(1\).png
        assets/SampleGrab.png
)

target_sources(SampleGrab
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(SampleGrab
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SampleGrab
    PRIVATE
        SampleGrabAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
