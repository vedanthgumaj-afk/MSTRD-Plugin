#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
// Exact color tokens from Figma Make
namespace Col {
// Body (from App.tsx body gradient)
static const juce::Colour bgTop{0xffe4e6e8}; // #e4e6e8
static const juce::Colour bgMid{0xffc4c6c8}; // #c4c6c8 at 40%
static const juce::Colour bgBot{0xffa8aab0}; // #a8aab0

static const juce::Colour textDark{0xff181a1c}; // #181a1c
static const juce::Colour dim{0xff505660};

// CRT screen  (from CRTScreen.tsx)
static const juce::Colour screenBg{0xff08101a};   // #08101a
static const juce::Colour screenGrid{0xff506e96}; // rgb(80,110,150) at α=0.15
static const juce::Colour screenText{0xff8a9aa8}; // #8a9aa8

// Curve — phosphor (from CRTScreen.tsx stroke #48b4d8)
static const juce::Colour phosphorCyan{0xff48b4d8};
static const juce::Colour phosphorOrange{0xffd88c28};
static const juce::Colour phosphorGreen{0xff28a468};

// Knob arc colors (from App.tsx knobDefaults)
static const juce::Colour blue{0xff4884d4};   // DRIVE    #4884d4
static const juce::Colour orange{0xffd88c28}; // THRESH   #d88c28
static const juce::Colour green{0xff28a468};  // POSTGAIN #28a468
static const juce::Colour grey{0xff50545c};   // MIX/KNEE #50545c

// Knob dot/glow colors (from App.tsx dotColor)
static const juce::Colour blueDot{0xff589ce8};
static const juce::Colour orangeDot{0xfff0a840};
static const juce::Colour greenDot{0xff38cc84};
static const juce::Colour greyDot{0xff808894};

// Knob ring (from RotaryKnob.tsx gradients)
static const juce::Colour ringDark{0xff808488};  // ring-grad-outer top
static const juce::Colour ringLight{0xffd4d6d8}; // ring-grad-outer bottom
static const juce::Colour wellBg{0xff181a1c};    // dark well

// Meter (from LevelMeter.tsx)
static const juce::Colour meterBg{0xff080a0c};
static const juce::Colour meterBar{0xff101418};
static const juce::Colour meterLbl{0xffa0a4a8};
static const juce::Colour meterTick{0xffd0d4d8};

inline juce::Colour modeColour(ClipMode m) {
  switch (m) {
  case ClipMode::CLEAN:
    return phosphorCyan;
  case ClipMode::KNOCK:
    return phosphorCyan;
  case ClipMode::VD:
    return phosphorOrange;
  case ClipMode::GRIT:
    return phosphorGreen;
  }
  return phosphorCyan;
}

inline juce::Colour knobArcColour(const juce::String &id) {
  if (id == "drive")
    return blue;
  if (id == "thresh")
    return orange;
  if (id == "post")
    return green;
  if (id == "asym")
    return orange;
  return grey;
}

inline juce::Colour knobDotColour(const juce::String &id) {
  if (id == "drive")
    return blueDot;
  if (id == "thresh")
    return orangeDot;
  if (id == "post")
    return greenDot;
  if (id == "asym")
    return orangeDot;
  return greyDot;
}
} // namespace Col

//==============================================================================
// Layout — proportional to Figma Make (1023×770) scaled to 580×480
static constexpr int kHeaderH = 60;    // 96/770 * 480 ≈ 60
static constexpr int kPresetBarH = 28; // 44/770 * 480 ≈ 27
static constexpr int kPad = 10;
static constexpr int kScreenH = 248; // 440/770 * 480 - adjustments
static constexpr int kMeterW = 62;   // 120/1023 * 580 ≈ 68, bit narrower
static constexpr int kKnobAreaH = 130;

static constexpr int W = 580;
static constexpr int H =
    kHeaderH + kPresetBarH + kPad * 2 + kScreenH + kKnobAreaH;
// 60+28+20+248+130 = 486

//==============================================================================
// LookAndFeel
//==============================================================================
SoftClipperLookAndFeel::SoftClipperLookAndFeel() {
  knobImg = juce::ImageCache::getFromMemory(BinaryData::knob_image_png,
                                            BinaryData::knob_image_pngSize);
}

void SoftClipperLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y,
                                              int w, int h, float sliderPos,
                                              float startAngle, float endAngle,
                                              juce::Slider &slider) {
  const float cx = x + w * 0.5f;
  const float cy = y + h * 0.5f;
  const float R = juce::jmin(w, h) * 0.44f;
  const float angle = startAngle + sliderPos * (endAngle - startAngle);

  const juce::String id = slider.getComponentID();
  const juce::Colour arcCol = Col::knobArcColour(id);
  const juce::Colour dotCol = Col::knobDotColour(id);

  // Sizes scaled from Figma (SVG 96px, half=48 → our R = r/44*R)
  const float r2 = R * 0.955f;    // inner ring
  const float rWell = R * 0.932f; // dark well
  const float arcR = R * 0.864f;  // arc track — inside the well
  const float capR = R * 0.636f;  // cap
  const float indR = R * 0.409f;  // indicator

  // ── 1. Outer metallic ring (dark-top → light-bottom) ──────────────────────
  {
    juce::ColourGradient og(Col::ringDark, cx, cy - R, Col::ringLight, cx,
                            cy + R, false);
    g.setGradientFill(og);
    g.fillEllipse(cx - R, cy - R, R * 2.0f, R * 2.0f);
  }

  // ── 2. Inner bevel ring (light-top → dark-bottom) ─────────────────────────
  {
    juce::ColourGradient ig(Col::ringLight, cx, cy - r2, Col::ringDark, cx,
                            cy + r2, false);
    g.setGradientFill(ig);
    g.fillEllipse(cx - r2, cy - r2, r2 * 2.0f, r2 * 2.0f);
  }

  // ── 3. Dark well ──────────────────────────────────────────────────────────
  g.setColour(Col::wellBg);
  g.fillEllipse(cx - rWell, cy - rWell, rWell * 2.0f, rWell * 2.0f);

  // ── 4. Arc track (dark channel inside well) ────────────────────────────────
  {
    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff202428));
    g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
  }

  // ── 5. Arc fill (colored, with glow) ──────────────────────────────────────
  if (sliderPos > 0.004f) {
    juce::Path fill;
    fill.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, angle, true);
    // Wide soft halo
    g.setColour(arcCol.withAlpha(0.22f));
    g.strokePath(fill, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    // Main arc
    g.setColour(arcCol);
    g.strokePath(fill, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    // Bright top edge
    g.setColour(arcCol.brighter(0.5f));
    g.strokePath(fill, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
  }

  // ── 6. Cap drop shadow ─────────────────────────────────────────────────────
  g.setColour(juce::Colour(0x70000000));
  g.fillEllipse(cx - capR - 0.5f, cy - capR + 2.0f, (capR + 0.5f) * 2.0f,
                (capR + 0.5f) * 2.0f);

  // ── 7. Dark plastic cap ────────────────────────────────────────────────────
  {
    juce::ColourGradient cap(juce::Colour(0xff4a4d52), cx - capR * 0.1f,
                             cy - capR * 0.55f, juce::Colour(0xff111214),
                             cx + capR * 0.1f, cy + capR * 0.75f, false);
    g.setGradientFill(cap);
    g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawEllipse(cx - capR + 0.5f, cy - capR + 0.5f, capR * 2.0f - 1.0f,
                  capR * 2.0f - 1.0f, 0.8f);
  }

  // ── 8. Indicator dot (JUCE: angle 0 = 12 o'clock, CW → sin/−cos) ──────────
  {
    const float dotX = cx + std::sin(angle) * indR;
    const float dotY = cy - std::cos(angle) * indR;
    const float dr = 3.0f;
    g.setColour(dotCol.withAlpha(0.55f));
    g.fillEllipse(dotX - dr * 2.2f, dotY - dr * 2.2f, dr * 4.4f, dr * 4.4f);
    g.setColour(dotCol);
    g.fillEllipse(dotX - dr, dotY - dr, dr * 2.0f, dr * 2.0f);
  }
}

void SoftClipperLookAndFeel::drawLabel(juce::Graphics &g, juce::Label &lbl) {
  g.fillAll(juce::Colours::transparentBlack);
  g.setColour(lbl.findColour(juce::Label::textColourId));
  g.setFont(lbl.getFont());
  g.drawFittedText(lbl.getText(), lbl.getLocalBounds().reduced(2),
                   lbl.getJustificationType(), 1);
}

void SoftClipperLookAndFeel::drawButtonBackground(juce::Graphics &g,
                                                  juce::Button &button,
                                                  const juce::Colour &,
                                                  bool /*isHovered*/,
                                                  bool isButtonDown) {
  auto b = button.getLocalBounds().toFloat();
  float corner = 3.0f;

  // Figma button: linear-gradient(180deg, #e6e8e8 0%, #c4c6c6 50%, #adb0b0
  // 100%)
  juce::Colour t =
      isButtonDown ? juce::Colour(0xffa0a5a8) : juce::Colour(0xffe6e8e8);
  juce::Colour m =
      isButtonDown ? juce::Colour(0xffb0b4b4) : juce::Colour(0xffc4c6c6);
  juce::Colour bot =
      isButtonDown ? juce::Colour(0xffb8bcbc) : juce::Colour(0xffadb0b0);
  (void)m;

  juce::ColourGradient grad(t, 0, 0, bot, 0, b.getHeight(), false);
  g.setGradientFill(grad);
  g.fillRoundedRectangle(b.reduced(1.0f), corner);

  // Border: #6a6c6c
  g.setColour(juce::Colour(0xff6a6c6c));
  g.drawRoundedRectangle(b.reduced(0.5f), corner, 1.0f);

  if (!isButtonDown) {
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawRoundedRectangle(b.reduced(1.5f), corner - 1.0f, 0.7f);
  }
}

void SoftClipperLookAndFeel::drawButtonText(juce::Graphics &g,
                                            juce::TextButton &button, bool,
                                            bool isButtonDown) {
  g.setFont(getTextButtonFont(button, button.getHeight()));
  g.setColour(juce::Colour(0xff202224));
  auto bounds = button.getLocalBounds().toFloat();
  if (isButtonDown)
    bounds.translate(0.0f, 1.0f);
  g.drawText(button.getButtonText(), bounds, juce::Justification::centred,
             true);
}

void SoftClipperLookAndFeel::drawComboBox(juce::Graphics &g, int w, int h,
                                          bool isButtonDown, int /*bX*/,
                                          int /*bY*/, int buttonW, int /*bH*/,
                                          juce::ComboBox & /*box*/) {
  auto b = juce::Rectangle<float>(0, 0, w, h);

  // Figma: #fdfdfd, border #7a7c80
  g.setColour(juce::Colour(0xfffdfdfd));
  g.fillRoundedRectangle(b, 3.0f);
  g.setColour(juce::Colour(0xff7a7c80));
  g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

  // Arrow area: linear-gradient(180deg, #d4dbf2 0%, #90a0d4 100%)
  juce::Rectangle<int> az(w - buttonW, 0, buttonW, h);
  juce::Colour at =
      isButtonDown ? juce::Colour(0xffb0bcd8) : juce::Colour(0xffd4dbf2);
  juce::Colour ab =
      isButtonDown ? juce::Colour(0xff7888b8) : juce::Colour(0xff90a0d4);
  juce::ColourGradient ag(at, 0, 0, ab, 0, h, false);
  g.setGradientFill(ag);
  g.fillRect(az.toFloat());

  g.setColour(juce::Colour(0xff7a7c80));
  g.drawVerticalLine(w - buttonW, 0, h);

  // Triangle arrow: fill #202a40
  juce::Path arrow;
  float ax = w - buttonW * 0.5f, ay = h * 0.5f;
  arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.5f);
  g.setColour(juce::Colour(0xff202a40));
  g.fillPath(arrow);
}

void SoftClipperLookAndFeel::positionComboBoxText(juce::ComboBox &box,
                                                  juce::Label &label) {
  label.setBounds(1, 1, box.getWidth() - box.getHeight(), box.getHeight() - 2);
  label.setFont(
      juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::plain)));
  label.setColour(juce::Label::textColourId, juce::Colour(0xff111111));
}

//==============================================================================
// TransferCurveComponent
//==============================================================================
TransferCurveComponent::TransferCurveComponent() {}

void TransferCurveComponent::mouseDown(const juce::MouseEvent &) {
  if (onModeClick)
    onModeClick();
}

void TransferCurveComponent::setParameters(float thDb, float pgDb, float drvDb,
                                           ClipMode m) {
  thresholdDb = thDb;
  postGainDb = pgDb;
  driveDb = drvDb;
  mode = m;
  repaint();
}

void TransferCurveComponent::paint(juce::Graphics &g) {
  const auto b = getLocalBounds();
  const float fW = (float)b.getWidth();
  const float fH = (float)b.getHeight();

  // ── Outer metallic bezel (from App.tsx bezel: #b8bcc0 → #909498) ──────────
  {
    juce::ColourGradient frame(juce::Colour(0xffb8bcc0), 0, 0,
                               juce::Colour(0xff909498), fW, fH, false);
    g.setGradientFill(frame);
    g.fillRoundedRectangle(b.toFloat(), 14.0f);

    // inset top highlight
    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 14.0f, 1.0f);
    // shadow bottom
    g.setColour(juce::Colour(0xff404442));
    g.drawRoundedRectangle(b.toFloat().reduced(1.5f), 12.5f, 1.3f);
  }

  // ── Inner dark shadow ring (from App.tsx: #121418, padding 16px) ─────────
  {
    auto innerB = b.reduced(8).toFloat();
    g.setColour(juce::Colour(0xff121418));
    g.fillRoundedRectangle(innerB, 10.0f);
    g.setColour(juce::Colour(0xff242628));
    g.drawRoundedRectangle(innerB, 10.0f, 2.0f);
  }

  // ── Screen glass (from CRTScreen.tsx: #08101a) ───────────────────────────
  auto screenB = b.reduced(12).toFloat();
  {
    juce::ColourGradient sg(juce::Colour(0xff0a1220), 0, 0,
                            juce::Colour(0xff050c14), 0, fH, false);
    g.setGradientFill(sg);
    g.fillRoundedRectangle(screenB, 8.0f);
    g.setColour(juce::Colour(0xff10181e));
    g.drawRoundedRectangle(screenB, 8.0f, 0.8f);
  }

  g.reduceClipRegion(screenB.toNearestInt());

  const float sx = screenB.getX();
  const float sy = screenB.getY();
  const float sw = screenB.getWidth();
  const float sh = screenB.getHeight();
  const float xi = sx + 8.0f;
  const float yi = sy + 8.0f;
  const float cw = sw - 16.0f;
  const float ch = sh - 16.0f;

  // ── CRT scanlines (from CRTScreen: rgba(255,255,255,0.02), 4px period) ─────
  g.setColour(juce::Colours::white.withAlpha(0.025f));
  for (float scanY = screenB.getY(); scanY < screenB.getBottom(); scanY += 4.0f)
    g.drawHorizontalLine((int)scanY, screenB.getX(), screenB.getRight());

  // ── Radial vignette (from CRTScreen: transparent 40% → rgba(0,0,0,0.4)) ──
  {
    // Horizontal edges
    juce::ColourGradient vgL(juce::Colour(0x60000000), sx, 0,
                             juce::Colours::transparentBlack, sx + 50.0f, 0,
                             false);
    g.setGradientFill(vgL);
    g.fillRect(screenB);
    juce::ColourGradient vgR(juce::Colour(0x60000000), sx + sw, 0,
                             juce::Colours::transparentBlack, sx + sw - 50.0f,
                             0, false);
    g.setGradientFill(vgR);
    g.fillRect(screenB);
    // Top/bottom
    juce::ColourGradient vgT(juce::Colour(0x50000000), 0, sy,
                             juce::Colours::transparentBlack, 0, sy + 40.0f,
                             false);
    g.setGradientFill(vgT);
    g.fillRect(screenB);
    juce::ColourGradient vgB(juce::Colour(0x50000000), 0, sy + sh,
                             juce::Colours::transparentBlack, 0,
                             sy + sh - 40.0f, false);
    g.setGradientFill(vgB);
    g.fillRect(screenB);
  }

  // ── Top glass reflection (from CRTScreen: rgba(255,255,255,0.05)
  // 0%→transparent 100%, 40% height) ──
  {
    juce::ColourGradient refl(juce::Colours::white.withAlpha(0.05f), sx, sy,
                              juce::Colours::transparentWhite, sx,
                              sy + sh * 0.4f, false);
    g.setGradientFill(refl);
    g.fillRoundedRectangle(screenB.reduced(1.5f), 7.0f);
  }

  // ── Grid lines (from CRTScreen: rgba(80,110,150,0.15)) ───────────────────
  g.setColour(Col::screenGrid.withAlpha(0.15f));
  g.drawLine(xi + cw * 0.25f, yi, xi + cw * 0.25f, yi + ch, 1.0f);
  g.drawLine(xi + cw * 0.50f, yi, xi + cw * 0.50f, yi + ch, 1.0f);
  g.drawLine(xi + cw * 0.75f, yi, xi + cw * 0.75f, yi + ch, 1.0f);
  g.drawLine(xi, yi + ch * 0.25f, xi + cw, yi + ch * 0.25f, 1.0f);
  g.drawLine(xi, yi + ch * 0.50f, xi + cw, yi + ch * 0.50f, 1.0f);
  g.drawLine(xi, yi + ch * 0.75f, xi + cw, yi + ch * 0.75f, 1.0f);
  // Center axes slightly brighter (from CRTScreen: rgba(80,110,150,0.3))
  g.setColour(Col::screenGrid.withAlpha(0.30f));
  g.drawLine(xi + cw * 0.50f, yi, xi + cw * 0.50f, yi + ch, 1.0f);
  g.drawLine(xi, yi + ch * 0.50f, xi + cw, yi + ch * 0.50f, 1.0f);

  // ── Unity diagonal (from CRTScreen: rgba(180,60,40,0.5), strokeWidth 2) ───
  {
    juce::Path p;
    p.startNewSubPath(xi, yi + ch);
    p.lineTo(xi + cw, yi);
    float d[] = {5.0f, 4.0f};
    juce::PathStrokeType s(1.5f);
    s.createDashedStroke(p, p, d, 2);
    g.setColour(juce::Colour(0xffb43c28).withAlpha(0.5f));
    g.strokePath(p, s);
  }

  // ── Transfer curve ────────────────────────────────────────────────────────
  const float thresh = juce::Decibels::decibelsToGain(thresholdDb);
  const float post = juce::Decibels::decibelsToGain(postGainDb);
  const float driveGain = juce::Decibels::decibelsToGain(driveDb);
  const float invThresh = (thresh > 1e-6f) ? (1.0f / thresh) : 1e6f;
  auto curveCol = Col::modeColour(mode);

  {
    juce::Path curve;
    for (int px = 0; px < (int)cw; ++px) {
      float xn = (px / cw) * 2.0f - 1.0f, yn = 0.0f;
      switch (mode) {
      case ClipMode::CLEAN:
        yn = thresh * std::tanh(xn * invThresh) * post;
        break;
      case ClipMode::KNOCK: {
        float xd = xn * 1.4f * (1.0f + driveGain * 0.25f);
        yn = thresh * std::tanh(xd * invThresh) * post;
        break;
      }
      case ClipMode::VD: {
        const float asym = 0.08f;
        yn = thresh * std::tanh((xn * 1.15f + asym) * invThresh) * post -
             thresh * std::tanh(asym * invThresh) * post;
        break;
      }
      case ClipMode::GRIT: {
        const float ceiling = 2.0f, asym = 0.04f;
        float hv = ceiling * std::tanh(xn * driveGain / ceiling);
        float dcCorr = thresh * std::tanh(asym * invThresh);
        yn = thresh * std::tanh((hv + asym) * invThresh) * post - dcCorr * post;
        break;
      }
      }
      yn = juce::jlimit(-1.0f, 1.0f, yn);
      float sy2 = (1.0f - (yn + 1.0f) * 0.5f) * ch;
      if (px == 0)
        curve.startNewSubPath(xi + px, yi + sy2);
      else
        curve.lineTo(xi + px, yi + sy2);
    }

    // Figma glow filter: feGaussianBlur stdDeviation 3 + 8
    // Outer wide glow (σ=8)
    g.setColour(curveCol.withAlpha(0.12f));
    g.strokePath(curve,
                 juce::PathStrokeType(22.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    // Inner tighter glow (σ=3)
    g.setColour(curveCol.withAlpha(0.45f));
    g.strokePath(curve, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    // Core line (strokeWidth 4 in Figma)
    g.setColour(curveCol.brighter(0.55f));
    g.strokePath(curve, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
  }

  // ── Mode name (from CRTScreen: #6898d8, 18px bold) ───────────────────────
  const char *modeNames[] = {"CLEAN", "KNOCK", "VD", "GRIT"};
  const char *modeTags[] = {"open - transparent", "bright - crispy - snap",
                            "warm - smooth - body", "grit - crush - character"};

  g.setFont(juce::Font(juce::FontOptions("Arial", 15.0f, juce::Font::bold)));
  g.setColour(juce::Colour(0xff6898d8));
  g.drawText(modeNames[(int)mode], (int)(xi + 10), (int)(yi + 8), 120, 20,
             juce::Justification::left, false);

  g.setFont(juce::Font(juce::FontOptions("Arial", 11.5f, juce::Font::plain)));
  g.setColour(Col::screenText); // #8a9aa8
  g.drawText(modeTags[(int)mode], (int)(xi + 10), (int)(yi + 27), 220, 14,
             juce::Justification::left, false);

  // ── Axis labels ───────────────────────────────────────────────────────────
  g.setFont(juce::Font(juce::FontOptions("Arial", 11.0f, juce::Font::plain)));
  g.setColour(Col::screenText);
  g.drawText("-1", (int)(xi + 4), (int)(yi + ch - 17), 22, 14,
             juce::Justification::left);
  g.drawText("+1", (int)(xi + cw - 26), (int)(yi + 4), 22, 14,
             juce::Justification::right);
  g.drawText("0", (int)(xi + 4), (int)(yi + ch * 0.5f - 8), 20, 14,
             juce::Justification::left);
  g.drawText("0", (int)(xi + cw - 26), (int)(yi + ch * 0.5f - 8), 20, 14,
             juce::Justification::right);
  g.drawText("IN  OUT", (int)xi, (int)(yi + ch - 17), (int)cw, 14,
             juce::Justification::centred);
}

//==============================================================================
// LevelMeterComponent
//==============================================================================
LevelMeterComponent::LevelMeterComponent() {}

void LevelMeterComponent::setLevel(float l, float r, float grDecibels) {
  levelL = l;
  levelR = r;
  grDb = grDecibels;
  repaint();
}

void LevelMeterComponent::paint(juce::Graphics &g) {
  const auto b = getLocalBounds();
  const float fW = (float)b.getWidth();
  const float fH = (float)b.getHeight();

  // ── Bezel (same as CRT screen bezel) ─────────────────────────────────────
  {
    juce::ColourGradient frame(juce::Colour(0xffb8bcc0), 0, 0,
                               juce::Colour(0xff909498), fW, fH, false);
    g.setGradientFill(frame);
    g.fillRoundedRectangle(b.toFloat(), 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.70f));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 8.0f, 0.8f);
    g.setColour(juce::Colour(0xff404442));
    g.drawRoundedRectangle(b.toFloat().reduced(1.5f), 7.0f, 1.1f);
  }

  // Inner shadow
  auto innerB = b.reduced(5).toFloat();
  g.setColour(juce::Colour(0xff121418));
  g.fillRoundedRectangle(innerB, 6.0f);

  // Screen glass (from LevelMeter: #080a0c)
  auto screenB = b.reduced(8).toFloat();
  g.setColour(Col::meterBg);
  g.fillRoundedRectangle(screenB, 5.0f);

  // Scanlines
  g.setColour(juce::Colours::white.withAlpha(0.02f));
  for (float scanY = screenB.getY(); scanY < screenB.getBottom(); scanY += 4.0f)
    g.drawHorizontalLine((int)scanY, screenB.getX(), screenB.getRight());

  // ── L / R labels (#a0a4a8, 12px bold) ─────────────────────────────────────
  const int topPad = 20;
  const int botPad = 6;
  const int barW = 12; // Figma: 14px, scaled down a bit
  const int gap = 5;
  const int totalBW = 2 * barW + gap;
  const int startX =
      (int)screenB.getX() + ((int)screenB.getWidth() - totalBW) / 2;
  const int barTop = (int)screenB.getY() + topPad;
  const int barH = (int)screenB.getHeight() - topPad - botPad;

  g.setFont(juce::Font(juce::FontOptions("Arial", 11.0f, juce::Font::bold)));
  g.setColour(Col::meterLbl);
  g.drawText("L", startX, (int)screenB.getY() + 3, barW, 13,
             juce::Justification::centred);
  g.drawText("R", startX + barW + gap, (int)screenB.getY() + 3, barW, 13,
             juce::Justification::centred);

  // ── Meter bars (from LevelMeter.tsx) ─────────────────────────────────────
  auto drawBar = [&](int bx, float level) {
    // Bar background (#101418)
    g.setColour(Col::meterBar);
    g.fillRect(bx, barTop, barW, barH);

    float db = juce::Decibels::gainToDecibels(level);
    float pct = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    int fillH = (int)(barH * pct);

    if (fillH > 0) {
      // Figma: linear-gradient(to top, #00ff00 0%, #00ff00 60%, #ffff00 80%,
      // #ff0000 100%)
      juce::ColourGradient barGrad(
          juce::Colour(0xff00ff00), (float)bx, (float)(barTop + barH),
          juce::Colour(0xffff0000), (float)bx, (float)barTop, false);
      barGrad.addColour(0.60, juce::Colour(0xff00ff00));
      barGrad.addColour(0.80, juce::Colour(0xffffff00));
      g.setGradientFill(barGrad);
      int by = barTop + barH - fillH;
      g.fillRect(bx, by, barW, fillH);

      // LED segments (repeating every 3px, #080a0c dividers)
      g.setColour(Col::meterBg.withAlpha(0.9f));
      for (int sy = by; sy < barTop + barH; sy += 3)
        g.drawHorizontalLine(sy, bx, bx + barW);
    }
  };

  drawBar(startX, levelL);
  drawBar(startX + barW + gap, levelR);

  // ── Scale ticks (#d0d4d8, 11px) ───────────────────────────────────────────
  g.setFont(juce::Font(juce::FontOptions("Arial", 9.0f, juce::Font::plain)));
  auto drawTick = [&](float dbVal, const juce::String &text) {
    float pct = juce::jlimit(0.0f, 1.0f, (dbVal + 60.0f) / 60.0f);
    int ty = barTop + (int)((1.0f - pct) * barH);
    g.setColour(Col::meterTick.withAlpha(0.4f));
    g.drawHorizontalLine(ty, startX - 3, startX - 1);
    g.drawHorizontalLine(ty, startX + totalBW + 1, startX + totalBW + 3);
    if (text.isNotEmpty()) {
      g.setColour(Col::meterTick.withAlpha(0.85f));
      g.drawText(text, startX + totalBW + 4, ty - 5, 22, 10,
                 juce::Justification::left);
    }
  };
  drawTick(0.0f, "0");
  drawTick(-12.0f, "");
  drawTick(-24.0f, "");
  drawTick(-60.0f, "-inf");
}

//==============================================================================
// Editor
//==============================================================================
SoftClipperAudioProcessorEditor::SoftClipperAudioProcessorEditor(
    SoftClipperAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), presetManager(p) {
  setLookAndFeel(&lnf);

  addAndMakeVisible(transferCurve);
  addAndMakeVisible(levelMeter);

  auto lblFont =
      juce::Font(juce::FontOptions("Arial", 10.5f, juce::Font::bold));
  auto valFont =
      juce::Font(juce::FontOptions("Arial", 12.0f, juce::Font::plain));

  auto setupKnob = [&](juce::Slider &k, const juce::String &id) {
    k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    k.setComponentID(id);
    k.setPopupDisplayEnabled(false, false, this);
    addAndMakeVisible(k);
  };
  auto setupLabel = [&](juce::Label &l, const juce::String &text) {
    l.setText(text, juce::dontSendNotification);
    l.setFont(lblFont);
    l.setColour(juce::Label::textColourId, Col::textDark);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
  };
  auto setupValue = [&](juce::Label &l) {
    l.setFont(valFont);
    l.setColour(juce::Label::textColourId, Col::textDark);
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
  };

  setupKnob(driveKnob, "drive");
  setupLabel(driveLabel, "DRIVE");
  setupValue(driveValue);
  setupKnob(thresholdKnob, "thresh");
  setupLabel(thresholdLabel, "THRESHOLD");
  setupValue(thresholdValue);
  setupKnob(postGainKnob, "post");
  setupLabel(postGainLabel, "POST GAIN");
  setupValue(postGainValue);
  setupKnob(mixKnob, "mix");
  setupLabel(mixLabel, "MIX");
  setupValue(mixValue);
  setupKnob(asymKnob, "asym");
  setupLabel(asymLabel, "ASYM");
  setupValue(asymValue);
  setupKnob(kneeKnob, "knee");
  setupLabel(kneeLabel, "KNEE");
  setupValue(kneeValue);

  driveAttachment = std::make_unique<Attachment>(p.apvts, "drive", driveKnob);
  threshAttachment =
      std::make_unique<Attachment>(p.apvts, "threshold", thresholdKnob);
  postAttachment =
      std::make_unique<Attachment>(p.apvts, "postGain", postGainKnob);
  mixAttachment = std::make_unique<Attachment>(p.apvts, "mix", mixKnob);
  asymAttachment = std::make_unique<Attachment>(p.apvts, "asymmetry", asymKnob);
  kneeAttachment = std::make_unique<Attachment>(p.apvts, "kneeWidth", kneeKnob);

  driveKnob.setDoubleClickReturnValue(true, 6.0);
  thresholdKnob.setDoubleClickReturnValue(true, -6.0);
  postGainKnob.setDoubleClickReturnValue(true, 0.0);
  mixKnob.setDoubleClickReturnValue(true, 100.0);
  asymKnob.setDoubleClickReturnValue(true, 0.08);
  kneeKnob.setDoubleClickReturnValue(true, 0.0);

  presetCombo.onChange = [this] {
    int idx = presetCombo.getSelectedItemIndex();
    if (idx >= 0) {
      presetManager.loadPreset(idx);
      delBtn.setEnabled(!presetManager.isFactory(idx));
    }
  };
  addAndMakeVisible(presetCombo);

  for (auto *btn : {&saveBtn, &delBtn, &osToggleBtn})
    addAndMakeVisible(btn);

  osToggleBtn.onClick = [this] {
    const int newQ = (audioProcessor.osQuality.load() == 2) ? 1 : 2;
    audioProcessor.osQuality.store(newQ);
    audioProcessor.osNeedsReinit.store(true);
    osToggleBtn.setButtonText(newQ == 2 ? "4X OS" : "2X OS");
  };

  saveBtn.onClick = [this] {
    auto *aw = new juce::AlertWindow(
        "Save Preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", {});
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    aw->enterModalState(
        true, juce::ModalCallbackFunction::create([aw, this](int result) {
          if (result == 1) {
            auto name = aw->getTextEditorContents("name").trim();
            if (name.isNotEmpty()) {
              presetManager.savePreset(name);
              refreshPresetList();
              presetCombo.setSelectedItemIndex(presetManager.getNumPresets() -
                                                   1,
                                               juce::dontSendNotification);
            }
          }
        }),
        true);
  };

  delBtn.onClick = [this] {
    int idx = presetCombo.getSelectedItemIndex();
    if (!presetManager.isFactory(idx)) {
      presetManager.deletePreset(idx);
      refreshPresetList();
    }
  };

  refreshPresetList();
  setSize(W, H);
  startTimerHz(30);
}

SoftClipperAudioProcessorEditor::~SoftClipperAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

//==============================================================================
void SoftClipperAudioProcessorEditor::paint(juce::Graphics &g) {
  // ── Body: 3-stop brushed aluminum (from App.tsx body gradient) ───────────
  {
    // Approximate 3-stop with 2-stop: top #e4e6e8 → bot #a8aab0
    juce::ColourGradient bg(Col::bgTop, 0.f, 0.f, Col::bgBot, 0.f,
                            (float)getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();

    // Brushed metal — vertical grain (from App.tsx repeating-linear-gradient)
    for (int y = 0; y < getHeight(); y += 2) {
      g.setColour(juce::Colours::white.withAlpha(0.05f));
      g.drawHorizontalLine(y, 0.f, (float)getWidth());
    }
  }

  // ── Header (same body gradient, separated by borderBottom) ───────────────
  {
    // The header is the body gradient top region — no extra fill needed.
    // Just draw a bottom border to separate (from App.tsx borderBottom: 2px
    // solid rgba(0,0,0,0.15))
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawHorizontalLine(kHeaderH - 2, 0, (float)getWidth());
    g.drawHorizontalLine(kHeaderH - 1, 0, (float)getWidth());
    // Highlight just below separator
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawHorizontalLine(kHeaderH, 0, (float)getWidth());
  }

  // ── MSHF gold hexagon (from App.tsx SVG, gold gradient) ───────────────────
  {
    juce::Path hex;
    const float hcx = 44.f, hcy = kHeaderH * 0.5f;
    const float hr = 20.f;
    for (int i = 0; i < 6; ++i) {
      float a = juce::MathConstants<float>::pi / 3.0f * i -
                juce::MathConstants<float>::pi / 6.0f;
      if (i == 0)
        hex.startNewSubPath(hcx + hr * std::cos(a), hcy + hr * std::sin(a));
      else
        hex.lineTo(hcx + hr * std::cos(a), hcy + hr * std::sin(a));
    }
    hex.closeSubPath();

    // Outer bevel (#fff8df → #805b15)
    juce::ColourGradient bevel(juce::Colour(0xfffff8df), hcx, hcy - hr,
                               juce::Colour(0xff805b15), hcx, hcy + hr, false);
    g.setGradientFill(bevel);
    g.fillPath(hex);

    // Inner face, shrunk slightly (#fbe29f → #9a6e1f → #6b4c11)
    juce::Path inner = hex;
    juce::ColourGradient gold(juce::Colour(0xfffbe29f), hcx, hcy - hr * 0.9f,
                              juce::Colour(0xff6b4c11), hcx, hcy + hr * 0.9f,
                              false);
    gold.addColour(0.40, juce::Colour(0xffd5a043));
    gold.addColour(0.80, juce::Colour(0xff9a6e1f));
    g.setGradientFill(gold);
    g.fillPath(inner, juce::AffineTransform::scale(0.94f, 0.94f, hcx, hcy));

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillPath(hex, juce::AffineTransform::translation(1.5f, 3.0f));
    // Re-paint actual hex on top of shadow
    g.setGradientFill(bevel);
    g.fillPath(hex);
    g.setGradientFill(gold);
    g.fillPath(inner, juce::AffineTransform::scale(0.94f, 0.94f, hcx, hcy));

    // Label "#4a320b", 14px, weight 900
    g.setColour(juce::Colour(0xff4a320b));
    g.setFont(juce::Font(juce::FontOptions("Arial", 9.5f, juce::Font::bold)));
    g.drawText("MSHF", (int)hcx - 16, (int)hcy - 6, 32, 13,
               juce::Justification::centred);
  }

  // ── Title: "MSTRD" 52px 800 #181a1c, "v1.0 (tanh)" 32px 500 #181a1c ──────
  g.setColour(juce::Colour(0xff181a1c));
  g.setFont(juce::Font(juce::FontOptions("Arial", 40.0f, juce::Font::bold)));
  g.drawText("MSTRD", 76, 8, 210, 46, juce::Justification::centredLeft);

  g.setFont(juce::Font(juce::FontOptions("Arial", 22.0f, juce::Font::plain)));
  g.drawText("v1.0", 236, 18, 170, 26, juce::Justification::centredLeft);

  // Underline (from App.tsx: left:112px, width:880px, 2px, gradient black
  // 30%→10%)
  {
    juce::ColourGradient ul(juce::Colours::black.withAlpha(0.30f), 74.f, 0,
                            juce::Colours::black.withAlpha(0.05f),
                            (float)getWidth(), 0, false);
    g.setGradientFill(ul);
    g.fillRect(74, kHeaderH - 6, getWidth() - 74, 2);
    // below highlight
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.fillRect(74, kHeaderH - 4, getWidth() - 74, 1);
  }

  // ── Preset bar (from App.tsx: #d8dade → #c0c2c6) ─────────────────────────
  {
    juce::ColourGradient pb(juce::Colour(0xffd8dade), 0.f,
                            (float)(kHeaderH + 2), juce::Colour(0xffc0c2c6),
                            0.f, (float)(kHeaderH + 2 + kPresetBarH), false);
    g.setGradientFill(pb);
    g.fillRect(0, kHeaderH + 2, getWidth(), kPresetBarH);

    // Bottom border: 1px rgba(0,0,0,0.3)
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.drawHorizontalLine(kHeaderH + 2 + kPresetBarH - 1, 0, (float)getWidth());
    // Highlight below
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawHorizontalLine(kHeaderH + 2 + kPresetBarH, 0, (float)getWidth());
  }

  const int meterLblY = kHeaderH + 2 + kPresetBarH + kPad + kScreenH + 6;

  // ── Vertical separator POST GAIN / MIX (from App.tsx: rgba(0,0,0,0.15) +
  // right rgba(255,255,255,0.4)) ──
  const int slotW = (W - kPad * 2) / 6;
  const int knobsY = meterLblY + 15;
  const float sepX = (float)(kPad + slotW * 3);
  g.setColour(juce::Colours::black.withAlpha(0.15f));
  g.fillRect((int)sepX, knobsY, 2, getHeight() - knobsY - 8);
  g.setColour(juce::Colours::white.withAlpha(0.40f));
  g.fillRect((int)sepX + 2, knobsY, 1, getHeight() - knobsY - 8);

  // ── Outer border ──────────────────────────────────────────────────────────
  g.setColour(juce::Colour(0xff101212));
  g.drawRoundedRectangle(1.f, 1.f, getWidth() - 2.f, getHeight() - 2.f, 5.0f,
                         2.0f);
}

void SoftClipperAudioProcessorEditor::resized() {
  const int presetY = kHeaderH + 2 + 2;
  const int btnH = 22;
  const int gap = 6;

  const int bwSave = 50, bwDel = 40, bwOS = 42;
  int rightW = gap + bwSave + gap + bwDel + gap + bwOS + 10;
  int comboW = W - 20 - rightW;

  int cx = 8;
  presetCombo.setBounds(cx, presetY, comboW, btnH);
  cx += comboW + gap;
  saveBtn.setBounds(cx, presetY, bwSave, btnH);
  cx += bwSave + gap;
  delBtn.setBounds(cx, presetY, bwDel, btnH);
  cx += bwDel + gap;
  osToggleBtn.setBounds(cx, presetY, bwOS, btnH);

  // Screen + meter
  const int screenTop = kHeaderH + 2 + kPresetBarH + kPad;
  const int curveW = W - kPad * 2 - kMeterW - 8;
  transferCurve.setBounds(kPad, screenTop, curveW, kScreenH);
  levelMeter.setBounds(W - kPad - kMeterW, screenTop, kMeterW, kScreenH);

  // Knobs — 6 equal slots
  const int knobsTop = screenTop + kScreenH + 8;
  const int slotW = (W - kPad * 2) / 6;
  const int knobSz = 80;
  const int labelH = 13;
  const int valH = 15;

  auto placeKnob = [&](int i, juce::Label &lbl, juce::Slider &k,
                       juce::Label &val) {
    int sx = kPad + i * slotW;
    int inset = (slotW - knobSz) / 2;
    lbl.setBounds(sx, knobsTop + 3, slotW, labelH);
    k.setBounds(sx + inset, knobsTop + labelH + 5, knobSz, knobSz);
    val.setBounds(sx, knobsTop + labelH + 5 + knobSz, slotW, valH);
  };

  placeKnob(0, driveLabel, driveKnob, driveValue);
  placeKnob(1, thresholdLabel, thresholdKnob, thresholdValue);
  placeKnob(2, postGainLabel, postGainKnob, postGainValue);
  placeKnob(3, mixLabel, mixKnob, mixValue);
  placeKnob(4, asymLabel, asymKnob, asymValue);
  placeKnob(5, kneeLabel, kneeKnob, kneeValue);
}

//==============================================================================
void SoftClipperAudioProcessorEditor::timerCallback() {
  levelMeter.setLevel(audioProcessor.peakL.load(), audioProcessor.peakR.load(),
                      audioProcessor.gainReductionDb.load());

  ClipMode currentMode = static_cast<ClipMode>(audioProcessor.clipMode.load());
  float thDb = audioProcessor.apvts.getRawParameterValue("threshold")->load();
  float pgDb = audioProcessor.apvts.getRawParameterValue("postGain")->load();
  float drvDb = audioProcessor.apvts.getRawParameterValue("drive")->load();
  transferCurve.setParameters(thDb, pgDb, drvDb, currentMode);

  driveValue.setText(juce::String(drvDb, 1) + " dB",
                     juce::dontSendNotification);
  thresholdValue.setText(juce::String(thDb, 1) + " dB",
                         juce::dontSendNotification);
  postGainValue.setText((pgDb >= 0.f ? "+" : "") + juce::String(pgDb, 1) +
                            " dB",
                        juce::dontSendNotification);
  float mix = audioProcessor.apvts.getRawParameterValue("mix")->load();
  float asym = audioProcessor.apvts.getRawParameterValue("asymmetry")->load();
  float knee = audioProcessor.apvts.getRawParameterValue("kneeWidth")->load();
  mixValue.setText(juce::String((int)mix) + "%", juce::dontSendNotification);
  asymValue.setText(juce::String(asym, 3), juce::dontSendNotification);
  kneeValue.setText(juce::String(knee, 1) + " dB", juce::dontSendNotification);
  osToggleBtn.setButtonText(audioProcessor.osQuality.load() == 2 ? "4X OS"
                                                                 : "2X OS");
}

void SoftClipperAudioProcessorEditor::refreshPresetList() {
  const int prevIdx = presetCombo.getSelectedItemIndex();
  const int n = presetManager.getNumPresets();
  const int nFactory = PresetManager::kNumFactory;

  presetCombo.clear(juce::dontSendNotification);
  for (int i = 0; i < n; ++i) {
    if (i == nFactory && n > nFactory)
      presetCombo.addSeparator();
    presetCombo.addItem(presetManager.getPresetName(i), i + 1);
  }

  int idx = juce::jlimit(0, juce::jmax(0, n - 1), prevIdx);
  presetCombo.setSelectedItemIndex(idx, juce::dontSendNotification);
  delBtn.setEnabled(idx >= nFactory);
}
