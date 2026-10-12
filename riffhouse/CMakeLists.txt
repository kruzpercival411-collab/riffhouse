cmake_minimum_required(VERSION 3.22)
project(Riffhouse VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Riffhouse
    COMPANY_NAME "Riffhouse Audio"
    PLUGIN_MANUFACTURER_CODE Rfhs
    PLUGIN_CODE Rfst
    FORMATS VST3
    PRODUCT_NAME "Riffhouse Studio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(Riffhouse PRIVATE
    Source/Generator.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Riffhouse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UNIX AND NOT APPLE)
    target_compile_definitions(Riffhouse PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(Riffhouse
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- local offline test (not used by the Windows build)
option(RIFFHOUSE_TESTS "Build offline render test" OFF)
if(RIFFHOUSE_TESTS)
    juce_add_console_app(RiffhouseRender PRODUCT_NAME "RiffhouseRender")
    target_sources(RiffhouseRender PRIVATE tools/RenderTest.cpp Source/Generator.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(RiffhouseRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Riffhouse")
    target_link_libraries(RiffhouseRender PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics)
endif()
