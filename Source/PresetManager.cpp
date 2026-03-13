#include "PresetManager.h"

//==============================================================================
// Factory presets — one per mode + an init
//==============================================================================
const PresetManager::FactoryPreset PresetManager::kFactory[PresetManager::kNumFactory] =
{
    //  name              thresh  post    drive   mix    asym    knee   mode
    { "Init",             -6.0f,  0.0f,   6.0f, 100.f, 0.08f,  0.0f, (int)ClipMode::KNOCK },
    { "Clean Open",       -6.0f,  0.0f,   0.0f, 100.f, 0.08f,  0.0f, (int)ClipMode::CLEAN },
    { "Knock Snap",       -3.0f, -2.0f,  14.0f, 100.f, 0.08f,  0.0f, (int)ClipMode::KNOCK },
    { "VD Warmth",        -6.0f,  0.0f,   8.0f,  85.f, 0.15f,  0.0f, (int)ClipMode::VD   },
    { "Grit Crush",       -3.0f, -1.0f,  18.0f, 100.f, 0.08f,  6.0f, (int)ClipMode::GRIT },
};

//==============================================================================
PresetManager::PresetManager (SoftClipperAudioProcessor& p) : processor (p)
{
    userPresetsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("MSTRD")
                         .getChildFile ("Presets");

    // Fallback to ~/Documents if Application Support is not accessible
    if (! userPresetsDir.createDirectory())
    {
        userPresetsDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                             .getChildFile ("MSTRD")
                             .getChildFile ("Presets");
        userPresetsDir.createDirectory();
    }
}

//==============================================================================
int PresetManager::getNumPresets() const
{
    return kNumFactory + getUserPresetFiles().size();
}

juce::String PresetManager::getPresetName (int index) const
{
    if (index < kNumFactory)
        return kFactory[index].name;

    auto files = getUserPresetFiles();
    int  userIdx = index - kNumFactory;
    if (userIdx < files.size())
        return files[userIdx].getFileNameWithoutExtension();

    return {};
}

//==============================================================================
void PresetManager::loadPreset (int index)
{
    if (index < 0 || index >= getNumPresets()) return;

    if (index < kNumFactory)
    {
        const auto& fp = kFactory[index];
        setParam (processor.apvts, "threshold", fp.threshold);
        setParam (processor.apvts, "postGain",  fp.postGain);
        setParam (processor.apvts, "drive",     fp.drive);
        setParam (processor.apvts, "mix",       fp.mix);
        setParam (processor.apvts, "asymmetry", fp.asymmetry);
        setParam (processor.apvts, "kneeWidth", fp.kneeWidth);
        processor.clipMode.store (fp.mode);
    }
    else
    {
        auto files   = getUserPresetFiles();
        int  userIdx = index - kNumFactory;
        if (userIdx < 0 || userIdx >= files.size()) return;

        if (auto xml = juce::parseXML (files[userIdx]))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (state.hasProperty ("clipMode"))
                processor.clipMode.store ((int) state["clipMode"]);
            processor.apvts.replaceState (state);
        }
    }
}

//==============================================================================
void PresetManager::savePreset (const juce::String& name)
{
    if (name.isEmpty()) return;

    auto state = processor.apvts.copyState();
    state.setProperty ("clipMode", processor.clipMode.load(), nullptr);

    if (auto xml = state.createXml())
    {
        auto file = userPresetsDir.getChildFile (name + ".xml");
        file.replaceWithText (xml->toString());
    }
}

//==============================================================================
void PresetManager::deletePreset (int index)
{
    if (isFactory (index)) return;

    auto files   = getUserPresetFiles();
    int  userIdx = index - kNumFactory;
    if (userIdx >= 0 && userIdx < files.size())
        files[userIdx].deleteFile();
}

//==============================================================================
void PresetManager::setParam (juce::AudioProcessorValueTreeState& apvts,
                               const juce::String& id, float value)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id)))
        p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (value));
}

juce::Array<juce::File> PresetManager::getUserPresetFiles() const
{
    juce::Array<juce::File> files;
    userPresetsDir.findChildFiles (files, juce::File::findFiles, false, "*.xml");
    files.sort();
    return files;
}
