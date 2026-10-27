#pragma once

#include <array>

#include <juce_graphics/juce_graphics.h>

namespace md::tokens
{

// The studio console look: colours taken verbatim from the design.
namespace console
{
inline const juce::Colour faceTop{0xFF6F7A86};
inline const juce::Colour faceBottom{0xFF5D6773};
inline const juce::Colour legend{0xFFF3F1EA};
inline const juce::Colour displayCentre{0xFFFBF0CF};
inline const juce::Colour displayEdge{0xFFE5CF98};
inline const juce::Colour bezel{0xFF26292D};
inline const juce::Colour ink{0xFF2A2418};
inline const juce::Colour capChords{0xFF7B6BD6};
inline const juce::Colour capBass{0xFFD6527C};
inline const juce::Colour capMelody{0xFF3FB6C4};
inline const juce::Colour ledOn{0xFFFFCF4A};
inline const juce::Colour ledOff{0xFF4E4A3C};
inline const juce::Colour sprinkleTop{0xFFD84A3A};
inline const juce::Colour sprinkleBottom{0xFFA8301F};
inline const juce::Colour keyTop{0xFFF0ECE0};
inline const juce::Colour keyBottom{0xFFC9C2AD};
inline const juce::Colour keyShadow{0xFF2C2F33};
inline const juce::Colour keyDarkTop{0xFF3A3F46};
inline const juce::Colour keyDarkBottom{0xFF22262B};
inline const juce::Colour keyDarkShadow{0xFF15171A};
inline const juce::Colour walnutA{0xFF3D2516};
inline const juce::Colour walnutB{0xFF4B2E1B};
inline const juce::Colour walnutC{0xFF35200F};
inline const juce::Colour knobLight{0xFF4B4B4B};
inline const juce::Colour knobDark{0xFF141414};
inline const juce::Colour readoutGlowColour = juce::Colour(0xFFFFBE5A).withAlpha(0.6F);
inline const juce::Colour displayGlowColour = juce::Colour(0xFFAA6E1E).withAlpha(0.45F);
inline const juce::Colour knobCap{0xFF8A8F96};
inline const juce::Colour tagline{0xFFDFE3E8};
} // namespace console

// Type sizes in px.
inline constexpr float topLineSize = 13.0F; // popup menu text

// Motion, in ms.
inline constexpr double keyPressMs = 80.0;
inline constexpr double ledFadeMs = 120.0;

// A drag starts after the pointer moves more than this many px.
inline constexpr int dragThreshold = 4;

// Layout, in logical px from the window's top-left corner (visual spec, "Layout").
namespace layout
{
inline constexpr int windowWidth = 1040;
inline constexpr int windowHeight = 580;
inline constexpr float minScale = 0.8F;
inline constexpr float maxScale = 1.5F;

// The faceplate.
inline constexpr int cheekWidth = 26;
inline constexpr std::array<int, 3> walnutStripes{3, 4, 2};
inline constexpr float cheekShadowWidth = 3.0F;
inline constexpr float screwSize = 13.0F;
inline constexpr float screwInset = 8.0F; // from the face's edge
inline constexpr float screwSlotWidth = 2.0F;
inline constexpr float screwSlotAngle = 35.0F; // degrees
inline constexpr int wordmarkX = 54;
inline constexpr int headerCentreY = 44;
inline constexpr float wordmarkFaceSize = 30.0F;
inline constexpr float wordmarkFaceSpacing = 0.16F; // em
inline constexpr float taglineSize = 11.0F;
inline constexpr float taglineSpacing = 0.18F; // em
inline constexpr int taglineGap = 18;

// The header readouts: the group's box, then each window and its legend within it.
inline const juce::Rectangle<int> header{574, 24, 412, 40};
inline constexpr int legendHeight = 12;
inline constexpr int readoutY = 14; // from the top of the header box
inline constexpr int readoutHeight = 26;
inline constexpr std::array<int, 4> readoutX{0, 96, 232, 328};
inline constexpr std::array<int, 4> readoutWidth{84, 124, 84, 84};
inline constexpr float readoutBorder = 2.0F;
inline constexpr float readoutRadius = 2.0F;
inline constexpr float readoutTextSize = 15.0F;
inline constexpr float readoutTextPad = 2.0F;
inline constexpr float readoutGlow = 8.0F;
inline constexpr float legendSize = 9.5F;
inline constexpr float legendSpacing = 0.2F; // em

// The display: the bezel box, and everything inside it in window space.
inline const juce::Rectangle<int> display{54, 86, 932, 250};
inline constexpr float displayBorder = 3.0F;
inline constexpr float displayRadius = 5.0F;
inline constexpr float displayGlow = 16.0F;

inline constexpr int cellX = 166;
inline constexpr int cellPitch = 202;
inline constexpr int cellY = 98;
inline constexpr int cellHeight = 40;
inline constexpr float cellInset = 4.0F;
inline constexpr float cellRadius = 3.0F;
inline constexpr float cellOutline = 1.5F;
inline constexpr float cellNameSize = 14.0F;
inline constexpr float cellFeelSize = 10.0F;
inline constexpr float cellFeelAlpha = 0.6F;
inline constexpr float cellBarWidth = 40.0F;
inline constexpr float cellBarHeight = 3.0F;

inline constexpr int laneX = 62;
inline constexpr int laneWidth = 916;
inline constexpr int laneHeight = 52;
inline constexpr std::array<int, 3> laneY{144, 200, 256};
inline constexpr float laneRadius = 3.0F;
inline constexpr float laneSelectedAlpha = 0.08F;
inline constexpr int laneLabelPad = 4;
inline constexpr int laneLabelWidth = 96;
inline constexpr float laneNameSize = 10.0F;
inline constexpr float laneNameSpacing = 0.14F; // em
inline constexpr float lanePatternSize = 10.0F;
inline constexpr int rollX = 166;
inline constexpr int rollPadY = 2;
inline constexpr int rollWidth = 808;
inline constexpr int rollHeight = 48;
inline constexpr float barLineAlpha = 0.18F;
// Note height as a fraction of the roll height, per lane (chords, bass, melody).
inline constexpr std::array<float, 3> noteHeightFraction{0.11F, 0.18F, 0.14F};
inline constexpr int noteMinWidth = 2;
inline constexpr float noteRadius = 1.0F;
inline constexpr int playheadWidth = 1;

inline const juce::Rectangle<int> bottomLine{66, 312, 908, 18};
inline constexpr float bottomLineSize = 11.0F;

// The knob panel under the display: the feel knobs, a divider, the three level knobs, a divider
// and the volume knob, each placed by its face's centre.
inline const juce::Rectangle<int> knobPanel{54, 350, 932, 104};
inline constexpr float knobPanelRadius = 4.0F;
inline constexpr int knobWidth = 100;
inline constexpr int knobHeight = 90;
inline constexpr float knobDiameter = 40.0F;
inline constexpr float knobLegendGap =
    9.0F; // lines a continuous knob's legend up with a stepped one's
inline constexpr std::array<int, 4> knobCentreX{130, 248, 366, 484};
inline constexpr int knobCentreY = 404;
inline constexpr std::array<int, 2> dividerX{560, 880};
inline constexpr int dividerY = 368;
inline constexpr int dividerHeight = 68;
inline constexpr float dividerAlpha = 0.14F;
inline constexpr int levelKnobWidth = 84;
inline constexpr int levelKnobHeight = 80;
inline constexpr float levelKnobDiameter = 34.0F;
inline constexpr float levelLegendGap = 18.0F; // room for the "LEVEL" line under the lane name
inline constexpr std::array<int, 3> levelCentreX{640, 726, 812};
inline constexpr int levelCentreY = 410;
inline constexpr juce::Point<int> volumeCentre{936, 404};
inline constexpr float mutedAlpha = 0.35F; // a muted lane's display row

// The key row under the knob panel: the mute keys on the left under a vertical MUTE legend,
// then play, stop and sprinkle on the right. Every legend sits 8 px under its key.
inline constexpr int keyY = 470;
inline constexpr int keyHeight = 54;
inline constexpr int keyWidth = 78;
inline constexpr int muteLegendCentreX = 62;
inline constexpr std::array<int, 3> muteKeyX{74, 166, 258};
inline constexpr std::array<int, 2> transportKeyX{632, 724};
inline const juce::Rectangle<int> sprinkleKey{816, keyY, 170, keyHeight};
inline constexpr float keyRadius = 3.0F;
inline constexpr float sprinkleRadius = 5.0F;
inline constexpr float keyPressDrop = 1.0F;
inline constexpr float keyShadowDrop = 2.0F; // the hard shadow's offset at rest
inline constexpr float sprinkleShadowDrop = 3.0F;
inline constexpr float sprinkleTitleSize = 10.0F;
inline constexpr float sprinkleTitleSpacing = 0.18F; // em
inline constexpr float keyIconSize = 12.0F;
inline constexpr float ledSize = 6.0F;
inline constexpr float keyLedGap = 6.0F; // from the LED's lower edge to the key's top
inline constexpr float ledGlow = 6.0F;
inline constexpr float keyLegendGap = 8.0F; // from the key's bottom to the legend's top
inline constexpr float keyLegendOverhang = 16.0F;
inline constexpr int keyRepaintMargin = 24; // above and below a key, for its LED, shadow and legend
// The sender screen (visual spec, "Sender screen").
inline constexpr int senderWidth = 520;
inline constexpr int senderHeight = 240;
inline constexpr int senderTitleX = 50;
inline constexpr int senderTitleCentreY = 34;
inline constexpr float senderTitleSize = 20.0F;
inline constexpr float senderTaglineSize = 14.0F;
inline constexpr int senderTaglineGap = 14;
inline constexpr std::array<int, 3> senderKeyX{50, 134, 218};
inline constexpr int senderKeyY = 104;
inline constexpr int senderKeyWidth = 72;
inline constexpr int senderKeyHeight = 24;
inline constexpr int senderMainKeyX = 322;
inline constexpr int senderMainKeyWidth = 120;
inline const juce::Rectangle<int> senderStatus{50, 176, 420, 28};
inline constexpr float senderStatusSize = 10.0F;
inline constexpr float senderStatusInset = 10.0F; // the text from the bezel's outer edge

// The about panel: the display's inner rect, and its lines, centred, in order.
inline const juce::Rectangle<int> aboutPanel{57, 89, 926, 244};
inline constexpr int aboutLineGap = 12;
inline constexpr float aboutTitleSize = 26.0F;
inline constexpr float aboutVersionSize = 12.0F;
inline constexpr float aboutOwnerSize = 13.0F;
inline constexpr float aboutSmallSize = 11.0F;
inline constexpr float focusRingOffset = 3.0F;
inline constexpr float focusRingStroke = 2.0F; // the screen's focus outline, drawn just inside
} // namespace layout

} // namespace md::tokens
