cmake_minimum_required(VERSION 3.22)

project(GaelRackFx VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GaelRackFx
    COMPANY_NAME "GaelRack"
    BUNDLE_ID "com.gaelrack.fx"

PLUGIN_MANUFACTURER_CODE Glrk
    PLUGIN_CODE Grfx

PRODUCT_NAME "GaelRack Fx"
    DESCRIPTION "Mono and stereo vocal effects"

FORMATS VST3 Standalone

IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
)

juce_generate_juce_header(GaelRackFx)

target_sources(GaelRackFx PRIVATE
    GaelRackFx.cpp
)

target_compile_definitions(GaelRackFx PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(GaelRackFx PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
)

if(MSVC)
    target_compile_options(GaelRackFx PRIVATE /utf-8)
endif()
