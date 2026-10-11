cmake_minimum_required(VERSION 3.22)

project(Zenthesizer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: build the C++ runtime into the plugin, so it doesn't depend on the
# Visual C++ Redistributable being installed on the PC that loads it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Downloads JUCE 8.0.15 automatically on the first configure (about 100 MB).
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Zenthesizer
    COMPANY_NAME             "DIY Audio"         # same vendor as Echo Space and TimeTwista, so they group in Ableton
    PRODUCT_NAME             "Zenthesizer"
    PLUGIN_MANUFACTURER_CODE Diya
    PLUGIN_CODE              Zen1
    BUNDLE_ID                com.diyaudio.zenthesizer
    FORMATS                  VST3 Standalone
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Instrument Synth
    COPY_PLUGIN_AFTER_BUILD  FALSE)

# Fonts (SIL Open Font License, see Resources/OFL-*.txt)
juce_add_binary_data(ZenFonts SOURCES
    Resources/InstrumentSerif-Italic.ttf
    Resources/InstrumentSans-Medium.ttf
    Resources/InstrumentSans-SemiBold.ttf)

target_sources(Zenthesizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Zenthesizer PRIVATE Source)

target_compile_definitions(Zenthesizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Zenthesizer
    PRIVATE
        ZenFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Developer tool (off by default): renders the editor to PNG files for checking the look.
option(ZEN_RENDER_TOOL "Build the editor snapshot tool" OFF)
if(ZEN_RENDER_TOOL)
    juce_add_console_app(ZenRender PRODUCT_NAME "ZenRender")
    target_sources(ZenRender PRIVATE tools/render.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(ZenRender PRIVATE Source)
    target_compile_definitions(ZenRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
                                                 JucePlugin_Name="Zenthesizer" JucePlugin_IsSynth=1)
    target_link_libraries(ZenRender PRIVATE ZenFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
