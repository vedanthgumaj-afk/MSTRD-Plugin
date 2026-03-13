#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Manages factory presets (hard-coded) + user presets (XML on disk).
// All methods are intended to run on the message thread.
//==============================================================================
class PresetManager
{
public:
    explicit PresetManager (SoftClipperAudioProcessor& processor);

    // Total count (factory + user)
    int          getNumPresets()          const;
    juce::String getPresetName (int index) const;
    bool         isFactory     (int index) const { return index < kNumFactory; }

    // Load preset into processor state (called on message thread)
    void loadPreset   (int index);

    // Save current state as a named user preset (creates / overwrites file)
    void savePreset   (const juce::String& name);

    // Delete a user preset by index (no-op for factory presets)
    void deletePreset (int index);

    static constexpr int kNumFactory = 5;

private:
    SoftClipperAudioProcessor& processor;
    juce::File                 userPresetsDir;

    struct FactoryPreset
    {
        const char* name;
        float threshold, postGain, drive, mix, asymmetry, kneeWidth;
        int   mode;   // ClipMode cast to int
    };

    static const FactoryPreset kFactory[kNumFactory];

    // Helper: set an APVTS float parameter by ID (normalises value automatically)
    static void setParam (juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, float value);

    juce::Array<juce::File> getUserPresetFiles() const;
};
