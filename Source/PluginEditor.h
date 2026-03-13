#pragma once

#include "PluginProcessor.h"
#include "PresetManager.h"
#include <JuceHeader.h>

//==============================================================================
class SoftClipperLookAndFeel : public juce::LookAndFeel_V4 {
public:
  juce::Colour driveKnobColour{juce::Colour(0xff58b8ff)};
  juce::Image knobImg;

  SoftClipperLookAndFeel();

  void drawRotarySlider(juce::Graphics &, int x, int y, int w, int h,
                        float sliderPos, float startAngle, float endAngle,
                        juce::Slider &) override;

  void drawLabel(juce::Graphics &, juce::Label &) override;

  void drawButtonBackground(juce::Graphics &, juce::Button &,
                            const juce::Colour &, bool isHovered,
                            bool isButtonDown) override;

  void drawButtonText(juce::Graphics &, juce::TextButton &, bool isHovered,
                      bool isButtonDown) override;

  void drawComboBox(juce::Graphics &, int w, int h, bool isButtonDown,
                    int buttonX, int buttonY, int buttonW, int buttonH,
                    juce::ComboBox &) override;

  void positionComboBoxText(juce::ComboBox &, juce::Label &) override;
};

//==============================================================================
class TransferCurveComponent : public juce::Component {
public:
  TransferCurveComponent();
  void paint(juce::Graphics &) override;
  void mouseDown(const juce::MouseEvent &) override;
  void setParameters(float threshDb, float postGainDb, float driveDb,
                     ClipMode mode);

  std::function<void()> onModeClick;

private:
  float thresholdDb{-6.0f};
  float postGainDb{0.0f};
  float driveDb{6.0f};
  ClipMode mode{ClipMode::KNOCK};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransferCurveComponent)
};

//==============================================================================
class LevelMeterComponent : public juce::Component {
public:
  LevelMeterComponent();
  void paint(juce::Graphics &) override;
  void setLevel(float leftLinear, float rightLinear, float grDecibels);

private:
  float levelL{0.0f};
  float levelR{0.0f};
  float grDb{0.0f};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};

//==============================================================================
class SoftClipperAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer {
public:
  explicit SoftClipperAudioProcessorEditor(SoftClipperAudioProcessor &);
  ~SoftClipperAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override;
  void refreshPresetList();

  SoftClipperAudioProcessor &audioProcessor;
  PresetManager presetManager;
  SoftClipperLookAndFeel lnf;

  // Preset bar
  juce::ComboBox presetCombo;
  juce::TextButton saveBtn{"SAVE"}, delBtn{"DEL"}, osToggleBtn{"4X OS"};

  // All 6 knobs in a single row
  juce::Slider driveKnob, thresholdKnob, postGainKnob, mixKnob, asymKnob,
      kneeKnob;
  juce::Label driveLabel, thresholdLabel, postGainLabel, mixLabel, asymLabel,
      kneeLabel;
  juce::Label driveValue, thresholdValue, postGainValue, mixValue, asymValue,
      kneeValue;

  using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  std::unique_ptr<Attachment> driveAttachment, threshAttachment, postAttachment,
      mixAttachment;
  std::unique_ptr<Attachment> asymAttachment, kneeAttachment;

  TransferCurveComponent transferCurve;
  LevelMeterComponent levelMeter;

  float clipLedBrightness{0.f};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoftClipperAudioProcessorEditor)
};
