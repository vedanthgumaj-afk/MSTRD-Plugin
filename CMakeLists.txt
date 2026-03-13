cmake_minimum_required(VERSION 3.22)

project(Mstrd VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Fetch JUCE ───────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─── Plugin target ────────────────────────────────────────────
juce_add_plugin(Mstrd
    COMPANY_NAME                "MSHF Audio"
    COMPANY_WEBSITE             "https://github.com"
    BUNDLE_ID                   "com.mshfaudio.mstrd"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_MANUFACTURER_CODE    Mshf
    PLUGIN_CODE                 Mstr
    FORMATS                     VST3 AU           # AU only builds on macOS
    PRODUCT_NAME                "MSTRD"
    VST3_CATEGORIES             "Fx" "Dynamics"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"
)

# ─── Generate JuceHeader.h (required for #include <JuceHeader.h>) ──
juce_generate_juce_header(Mstrd)

# ─── Binary data (embedded images) ───────────────────────────
juce_add_binary_data(MstrdBinaryData SOURCES
    Source/knob_image.png
)
set_target_properties(MstrdBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ─── Sources ──────────────────────────────────────────────────
target_sources(Mstrd PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
)

# ─── Compile definitions ──────────────────────────────────────
target_compile_definitions(Mstrd
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ─── Link JUCE modules ────────────────────────────────────────
target_link_libraries(Mstrd
    PRIVATE
        MstrdBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ─── Packaging (CPack) ────────────────────────────────────────
set(CPACK_PACKAGE_NAME "MSTRD")
set(CPACK_PACKAGE_VENDOR "MSHF Audio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MSTRD Saturation Plugin")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")

if(APPLE)
    set(CPACK_GENERATOR "productbuild")
    # For macOS, we essentially just need to package the built .vst3 and .component output
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
else()
    set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
