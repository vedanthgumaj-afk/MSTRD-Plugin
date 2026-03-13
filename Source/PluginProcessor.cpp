#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace ParamIDs
{
    static const juce::ParameterID threshold  { "threshold",  1 };
    static const juce::ParameterID postGain   { "postGain",   1 };
    static const juce::ParameterID drive      { "drive",      1 };
    static const juce::ParameterID mix        { "mix",        1 };
    static const juce::ParameterID asymmetry  { "asymmetry",  1 };
    static const juce::ParameterID kneeWidth  { "kneeWidth",  1 };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
    SoftClipperAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::threshold, "Threshold",
        juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::postGain, "Post Gain",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) {
                return (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::drive, "Drive",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::mix, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (v, 0) + " %"; })));

    // Advanced — hidden in UI (Phase 2 will expose)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::asymmetry, "Asymmetry",
        juce::NormalisableRange<float> (0.0f, 0.3f, 0.001f), 0.08f,
        juce::AudioParameterFloatAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamIDs::kneeWidth, "Knee Width",
        juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

//==============================================================================
SoftClipperAudioProcessor::SoftClipperAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SoftClipperState", createParameterLayout())
{
    // Oversampler: 2 channels, factor=2 (2^2=4×), IIR polyphase, low latency
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false /* not max quality = lower latency */);
}

SoftClipperAudioProcessor::~SoftClipperAudioProcessor() {}

//==============================================================================
void SoftClipperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Smoothing ramps
    threshSmoothed  .reset (sampleRate, 0.020);
    postGainSmoothed.reset (sampleRate, 0.020);
    driveSmoothed   .reset (sampleRate, 0.015);
    mixSmoothed     .reset (sampleRate, 0.030);
    asymSmoothed    .reset (sampleRate, 0.025);

    // Snap to current param values
    threshSmoothed  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ParamIDs::threshold.getParamID())->load()));
    postGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ParamIDs::postGain.getParamID())->load()));
    driveSmoothed   .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ParamIDs::drive.getParamID())->load()));
    mixSmoothed     .setCurrentAndTargetValue (
        apvts.getRawParameterValue (ParamIDs::mix.getParamID())->load() / 100.0f);
    asymSmoothed    .setCurrentAndTargetValue (
        apvts.getRawParameterValue (ParamIDs::asymmetry.getParamID())->load());
    kneeSmoothed    .reset (sampleRate, 0.020);
    kneeSmoothed    .setCurrentAndTargetValue (
        apvts.getRawParameterValue (ParamIDs::kneeWidth.getParamID())->load());

    // Oversampler
    oversampler->initProcessing ((size_t) samplesPerBlock);
    oversampler->reset();

    lastBlockSize = samplesPerBlock;

    // Report latency to host
    setLatencySamples ((int) oversampler->getLatencyInSamples());

    // DC blockers
    for (auto& dc : dcBlockers) dc.reset();

    // KNOCK envelope follower coefficients (runs at original sample rate)
    const float sr = (float) sampleRate;
    efFastCoeff = 1.0f - std::exp (-1.0f / (sr * 0.001f));  // ~1 ms
    efSlowCoeff = 1.0f - std::exp (-1.0f / (sr * 0.010f));  // ~10 ms
    efFast.fill (0.0f);
    efSlow.fill (0.0f);

    vcaAttCoeff = 1.0f - std::exp (-1.0f / (sr * 0.001f));  // 1 ms attack
    vcaRelCoeff = 1.0f - std::exp (-1.0f / (sr * 0.150f));  // 150 ms base release (4× slower at peak)
    vcaEnv = 0.0f;

    meterDecayCoeff = 1.0f - std::exp (-1.0f / (sr * 0.3f));

    peakL.store (0.0f);
    peakR.store (0.0f);
    gainReductionDb.store (0.0f);
}

void SoftClipperAudioProcessor::releaseResources()
{
    oversampler->reset();
}

bool SoftClipperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

//==============================================================================
//
//  ══════════════════════════════════════════════════════════════
//  CLEAN mode  (transparent — symmetric tanh, no colouration)
//  ══════════════════════════════════════════════════════════════
//
//  Chain: tanh clip at threshold + post gain
//
//  ══════════════════════════════════════════════════════════════
//  KNOCK mode  (Carti / Ken Carson / Homixide / Destroy Lonely)
//  ══════════════════════════════════════════════════════════════
//
//  Dual-envelope-follower transient enhancement:
//    efFast tracks attack (1 ms), efSlow tracks sustain (10 ms)
//    Transient = efFast > efSlow → boost gain into clipper
//  Chain: transient boost → tanh → post gain
//
//  ══════════════════════════════════════════════════════════════
//  VD mode  (boom bap / old house / warm / vintage)
//  ══════════════════════════════════════════════════════════════
//
//  Asymmetric tanh (2nd-harmonic warmth) + DC blocker
//
//  ══════════════════════════════════════════════════════════════
//  GRIT / 3630 mode  (hardware crush — internal ceiling)
//  ══════════════════════════════════════════════════════════════
//
//  Internal headroom ceiling (≈ +6 dBFS = 2.0 linear):
//    drives into a pre-ceiling tanh then outputs with slight
//    asymmetry + DC blocker
//
//==============================================================================

void SoftClipperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // ── Oversampling quality hot-swap (message-thread request, audio-thread exec) ──
    if (osNeedsReinit.exchange (false) && lastBlockSize > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) osQuality.load(),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
        oversampler->initProcessing ((size_t) lastBlockSize);
        oversampler->reset();
        setLatencySamples ((int) oversampler->getLatencyInSamples());
        // Reset stateful DSP after oversampler change
        vcaEnv = 0.0f;
        efFast.fill (0.0f);
        efSlow.fill (0.0f);
        for (auto& dc : dcBlockers) dc.reset();
    }

    const ClipMode mode = static_cast<ClipMode> (clipMode.load());
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Update smoothed targets
    threshSmoothed  .setTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ParamIDs::threshold.getParamID())->load()));
    postGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ParamIDs::postGain.getParamID())->load()));
    driveSmoothed   .setTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ParamIDs::drive.getParamID())->load()));
    mixSmoothed     .setTargetValue (
        apvts.getRawParameterValue (ParamIDs::mix.getParamID())->load() / 100.0f);
    asymSmoothed    .setTargetValue (
        apvts.getRawParameterValue (ParamIDs::asymmetry.getParamID())->load());
    kneeSmoothed    .setTargetValue (
        apvts.getRawParameterValue (ParamIDs::kneeWidth.getParamID())->load());

    // Track input peaks before oversampling modifies the buffer
    float inputPeakTotal = 0.0f;
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            inputPeakTotal = juce::jmax (inputPeakTotal, std::abs (data[i]));
    }

    // ── Upsample ──────────────────────────────────────────────
    juce::dsp::AudioBlock<float> inputBlock (buffer);
    auto osBlock = oversampler->processSamplesUp (inputBlock);

    const int osNumSamples = (int) osBlock.getNumSamples();
    const int osFactor     = (numSamples > 0) ? (osNumSamples / numSamples) : 4;

    // ── Process at 4× rate ────────────────────────────────────
    // Smoothers advance at the original sample rate (once per original sample),
    // and the resulting values are held constant across the 4 oversampled sub-samples.
    float gritGrBlock = 1.0f;  // worst-case VCA gain this block (for GR meter)
    for (int i = 0; i < numSamples; ++i)
    {
        const float thresh    = threshSmoothed  .getNextValue();
        const float postGain  = postGainSmoothed.getNextValue();
        const float driveGain = driveSmoothed   .getNextValue();
        const float mixGain   = mixSmoothed     .getNextValue();
        const float asymmetry = asymSmoothed    .getNextValue();
        const float kneeDb    = kneeSmoothed    .getNextValue();
        const float invThresh = (thresh > 1e-6f) ? (1.0f / thresh) : 1e6f;

        // ── GRIT: stereo-linked sidechain + program-dependent release ─────────
        float vcaGainLinked = 1.0f;
        if (mode == ClipMode::GRIT)
        {
            // Sidechain = max(|L|, |R|) × drive — single envelope keeps stereo image stable
            float sidechain = 0.0f;
            for (int c = 0; c < numChannels && c < 2; ++c)
                sidechain = juce::jmax (sidechain,
                    std::abs (osBlock.getChannelPointer ((size_t)c)[i * osFactor]) * driveGain);

            // Program-dependent release: up to 4× slower when heavily compressed
            const float envOverThresh = (thresh > 1e-6f)
                ? juce::jmax (0.0f, vcaEnv - thresh) / thresh : 0.0f;
            const float pdRelCoeff = vcaRelCoeff / (1.0f + 4.0f * juce::jmin (envOverThresh, 1.0f));

            vcaEnv += ((sidechain > vcaEnv) ? vcaAttCoeff : pdRelCoeff) * (sidechain - vcaEnv);

            if (vcaEnv > 1e-6f)
            {
                const float overDb   = juce::Decibels::gainToDecibels (vcaEnv)
                                     - juce::Decibels::gainToDecibels (thresh);
                const float halfKnee = kneeDb * 0.5f;
                float grDb = 0.0f;

                if (kneeDb < 0.1f || overDb > halfKnee)
                {
                    if (overDb > 0.0f)
                        grDb = overDb * (1.0f - 1.0f / gritRatio);
                }
                else if (overDb > -halfKnee)
                {
                    const float t        = (overDb + halfKnee) / kneeDb;
                    const float effRatio = 1.0f + (gritRatio - 1.0f) * t;
                    grDb = juce::jmax (0.0f, overDb) * (1.0f - 1.0f / effRatio);
                }

                vcaGainLinked = juce::Decibels::decibelsToGain (-grDb);
            }

            gritGrBlock = juce::jmin (gritGrBlock, vcaGainLinked);
        }

        for (int ch = 0; ch < numChannels && ch < 2; ++ch)
        {
            float* osData = osBlock.getChannelPointer ((size_t)ch);

            // KNOCK: per-channel envelope followers at original sample rate
            float efBoost = 1.0f;
            if (mode == ClipMode::KNOCK)
            {
                float absIn = std::abs (osData[i * osFactor]);
                efFast[(size_t)ch] += efFastCoeff * (absIn - efFast[(size_t)ch]);
                efSlow[(size_t)ch] += efSlowCoeff * (absIn - efSlow[(size_t)ch]);
                efBoost = 1.f + driveGain *
                    juce::jmax (0.f, efFast[(size_t)ch] - efSlow[(size_t)ch]) /
                    juce::jmax (efSlow[(size_t)ch], 1e-6f);
            }

            for (int j = 0; j < osFactor; ++j)
            {
                const int   osIdx     = i * osFactor + j;
                const float rawSample = osData[osIdx];
                float y = 0.0f;

                switch (mode)
                {
                    case ClipMode::CLEAN:
                    {
                        // Transparent tanh — symmetric, hard knee at threshold
                        y = thresh * std::tanh (rawSample * invThresh) * postGain;
                        break;
                    }

                    case ClipMode::KNOCK:
                    {
                        // Dual-EF transient enhance + tanh clip
                        float xd = rawSample * efBoost * 1.4f;
                        y = thresh * std::tanh (xd * invThresh) * postGain;
                        break;
                    }

                    case ClipMode::VD:
                    {
                        // Asymmetric tanh (2nd harmonic) + DC correction + DC blocker
                        float offset    = asymmetry;
                        float xs        = rawSample * 1.15f + offset;
                        float dcCorrect = thresh * std::tanh (offset * invThresh);
                        y = thresh * std::tanh (xs * invThresh) * postGain
                          - dcCorrect * postGain;
                        y = dcBlockers[(size_t)ch].process (y);
                        break;
                    }

                    case ClipMode::GRIT:
                    {
                        // VCA-compressed signal driven into headroom ceiling (≈ +6 dBFS)
                        const float ceiling   = 2.0f;
                        float compressed      = vcaGainLinked * rawSample * driveGain;
                        float headroomed      = ceiling * std::tanh (compressed / ceiling);

                        // Output soft clip with slight asymmetry (odd-harmonic bias, tiny 2nd)
                        const float asym3630  = 0.04f;
                        float asymIn          = headroomed + asym3630;
                        float dcCorr          = thresh * std::tanh (asym3630 * invThresh);
                        y = thresh * std::tanh (asymIn * invThresh) * postGain
                          - dcCorr * postGain;
                        y = dcBlockers[(size_t)ch].process (y);
                        break;
                    }
                }

                // Dry / wet mix
                y = (1.f - mixGain) * rawSample + mixGain * y;
                osData[osIdx] = y;
            }
        }
    }

    // ── Downsample ────────────────────────────────────────────
    oversampler->processSamplesDown (inputBlock);

    // ── Meter tracking (post-processing) ─────────────────────
    float peakLAcc = peakL.load();
    float peakRAcc = peakR.load();
    float outputPeakTotal = 0.0f;
    bool  clipped = false;

    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float absOut = std::abs (data[i]);
            if (absOut > outputPeakTotal) outputPeakTotal = absOut;
            if (absOut >= 0.99f) clipped = true;

            if (ch == 0)
            {
                peakLAcc += (absOut - peakLAcc) * meterDecayCoeff;
                if (absOut > peakLAcc) peakLAcc = absOut;
            }
            else
            {
                peakRAcc += (absOut - peakRAcc) * meterDecayCoeff;
                if (absOut > peakRAcc) peakRAcc = absOut;
            }
        }
    }

    peakL.store (peakLAcc);
    peakR.store (peakRAcc);
    if (clipped) clipLed.store (1.0f);

    // GR meter: VCA gain reduction for GRIT, output/input ratio for other modes
    if (mode == ClipMode::GRIT)
    {
        gainReductionDb.store (juce::Decibels::gainToDecibels (gritGrBlock));
    }
    else if (inputPeakTotal > 1e-6f && outputPeakTotal > 1e-6f)
    {
        float gr = juce::Decibels::gainToDecibels (outputPeakTotal)
                 - juce::Decibels::gainToDecibels (inputPeakTotal);
        gainReductionDb.store (juce::jmin (0.0f, gr));
    }
    else gainReductionDb.store (0.0f);
}

//==============================================================================
juce::AudioProcessorEditor* SoftClipperAudioProcessor::createEditor()
{
    return new SoftClipperAudioProcessorEditor (*this);
}

void SoftClipperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("clipMode", clipMode.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SoftClipperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto restored = juce::ValueTree::fromXml (*xml);
        if (restored.hasProperty ("clipMode"))
            clipMode.store (static_cast<int> (restored["clipMode"]));
        apvts.replaceState (restored);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoftClipperAudioProcessor();
}
