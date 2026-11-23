cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

project(TuxiroWarpMe VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # static runtime: no Visual C++ redistributable needed on the user's PC
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(TWM_BUILD_TESTS "Build the offline core tests (no JUCE needed)" OFF)
if(TWM_BUILD_TESTS)
    add_executable(twm_core_test Tests/CoreTest.cpp Source/BeatAnalyzer.cpp Source/TimeStretch.cpp)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TuxiroWarpMe
    PRODUCT_NAME "TUXIRO WARP ME"
    COMPANY_NAME "Tuxiro"
    BUNDLE_ID com.tuxiro.warpme
    PLUGIN_MANUFACTURER_CODE Txro
    PLUGIN_CODE Twpm
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TuxiroWarpMe PRIVATE
    Source/BeatAnalyzer.cpp
    Source/TimeStretch.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(TuxiroWarpMe PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(TuxiroWarpMe
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags)
