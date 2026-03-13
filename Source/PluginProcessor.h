#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

//==============================================================================
// Saturation mode
enum class ClipMode { CLEAN = 0, KNOCK = 1, VD = 2, GRIT = 3 };

//==============================================================================
class SoftClipperAudioProcessor  : public juce::AudioProcessor
{
public:
    SoftClipperAudioProcessor();
    ~SoftClipperAudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram  (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName  (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> peakL           { 0.0f };
    std::atomic<float> peakR           { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> clipLed         { 0.0f };  // set to 1.0 on clip; editor decays

    // Mode written by UI thread, read by audio thread
    std::atomic<int>   clipMode    { static_cast<int>(ClipMode::KNOCK) };

    // Oversampling quality: 1 = 2× (low CPU), 2 = 4× (default high quality)
    std::atomic<int>   osQuality   { 2 };
    std::atomic<bool>  osNeedsReinit { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::SmoothedValue<float> threshSmoothed;
    juce::SmoothedValue<float> postGainSmoothed;
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> asymSmoothed;
    juce::SmoothedValue<float> kneeSmoothed;

    // Oversampler (4× IIR polyphase — low latency)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // DC blocker (Julius O. Smith canonical) — one per channel
    struct DCBlocker
    {
        float xPrev = 0.f, yPrev = 0.f;
        static constexpr float R = 0.995f;

        float process (float x)
        {
            float y = x - xPrev + R * yPrev;
            xPrev = x; yPrev = y;
            return y;
        }
        void reset() { xPrev = yPrev = 0.f; }
    };
    std::array<DCBlocker, 2> dcBlockers;

    // KNOCK dual-envelope followers (fast + slow per channel)
    std::array<float, 2> efFast { 0.f, 0.f };
    std::array<float, 2> efSlow { 0.f, 0.f };
    float efFastCoeff { 0.f }, efSlowCoeff { 0.f };

    // GRIT VCA compressor — stereo-linked, program-dependent release
    float vcaEnv      { 0.f };   // single linked envelope (max L/R sidechain)
    float vcaAttCoeff { 0.f }, vcaRelCoeff { 0.f };
    static constexpr float gritRatio = 8.0f;

    int   lastBlockSize   { 0 };
    float meterDecayCoeff { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoftClipperAudioProcessor)
};
