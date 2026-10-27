#pragma once

#include <array>

#include <juce_graphics/juce_graphics.h>

namespace md::tokens
{

// Colours, named as in the visual spec's colour token table.
inline const juce::Colour body{0xFFDCDDE0};
inline const juce::Colour bodyEdge{0xFFB4B6BB};
inline const juce::Colour grille{0xFFCFD0D4};
inline const juce::Colour grilleEdge{0xFFBFC1C6};
inline const juce::Colour grilleDot{0xFF85878E};
inline const juce::Colour ink{0xFF24252B};
inline const juce::Colour inkQuiet{0xFF5E6068};
inline const juce::Colour inkValue{0xFF3B3D44};
inline const juce::Colour inkFaint{0xFF6A6C74};
inline const juce::Colour inkStatus{0xFF4A4C54};
inline const juce::Colour screen{0xFF050507};
inline const juce::Colour bezel{0xFF1B1C20};
inline const juce::Colour screenInk{0xFFF2F2F5};
inline const juce::Colour screenQuiet{0xFF8A8C96};
inline const juce::Colour screenAxis{0xFF15161B};
inline const juce::Colour screenBar{0xFF1A1B21};
inline const juce::Colour screenLine{0xFF2A2B32};
inline const juce::Colour screenSelected{0xFF121318};
inline const juce::Colour screenPanel{0xFF0D0E12};
inline const juce::Colour spellCurrent{0xFF1A1B22};
inline const juce::Colour knurlLight{0xFFCDCED2};
inline const juce::Colour knurlDark{0xFFA5A7AC};
inline const juce::Colour knurlEdge{0xFF8E9096};
inline const juce::Colour cyan{0xFF5FE3F0};
inline const juce::Colour violet{0xFFA68BFF};
inline const juce::Colour violetSoft{0xFFC9B8FF};
inline const juce::Colour violetDeep{0xFF2B1F5C};
inline const juce::Colour magenta{0xFFFF5CA8};
inline const juce::Colour gold{0xFFFFD166};
inline const juce::Colour onColour{0xFF050507};
inline const juce::Colour focus{0xFF8C6CF5};

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
inline const juce::Colour walnutA{0xFF3D2516};
inline const juce::Colour walnutB{0xFF4B2E1B};
inline const juce::Colour walnutC{0xFF35200F};
inline const juce::Colour knobLight{0xFF4B4B4B};
inline const juce::Colour knobDark{0xFF141414};
} // namespace console

// Body material.
inline constexpr float bodyRadius = 28.0F;
inline constexpr float bodyBorder = 1.0F;
inline constexpr float brushWhiteAlpha = 0.09F;
inline constexpr float brushBlackAlpha = 0.025F;
inline constexpr int brushPeriod = 3;
inline constexpr float highlightAlpha = 0.75F;

// Screen material.
inline constexpr float screenRadius = 20.0F;
inline constexpr float bezelWidth = 7.0F;

// Type sizes in px.
inline constexpr float topLineSize = 13.0F; // popup menu text
inline constexpr float statusLineSize = 11.0F;
inline constexpr float keyLabelSize = 10.0F; // text inside the sender keys
inline constexpr float wordmarkSize = 27.0F;
inline constexpr float wordmarkSpacing = -0.5F; // px between letters

// Motion, in ms.
inline constexpr double keyPressMs = 80.0;
inline constexpr double ledFadeMs = 120.0;

// A drag starts after the pointer moves more than this many px.
inline constexpr int dragThreshold = 4;

// Layout, in logical px from the window's top-left corner (visual spec, "Layout").
namespace layout
{
inline constexpr int windowWidth = 1040;
inline constexpr int windowHeight = 600;
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
inline const juce::Rectangle<int> display{54, 86, 672, 306};
inline constexpr float displayBorder = 3.0F;
inline constexpr float displayRadius = 5.0F;
inline constexpr float displayGlow = 16.0F;

inline constexpr int cellX = 166;
inline constexpr int cellPitch = 137;
inline constexpr int cellY = 98;
inline constexpr int cellHeight = 48;
inline constexpr float cellInset = 4.0F;
inline constexpr float cellRadius = 3.0F;
inline constexpr float cellOutline = 1.5F;
inline constexpr float cellNameSize = 14.0F;
inline constexpr float cellFeelSize = 10.0F;
inline constexpr float cellFeelAlpha = 0.6F;
inline constexpr float cellBarWidth = 40.0F;
inline constexpr float cellBarHeight = 3.0F;

inline constexpr int laneX = 62;
inline constexpr int laneWidth = 656;
inline constexpr int laneHeight = 64;
inline constexpr std::array<int, 3> laneY{154, 222, 290};
inline constexpr float laneRadius = 3.0F;
inline constexpr float laneSelectedAlpha = 0.08F;
inline constexpr int laneLabelPad = 4;
inline constexpr int laneLabelWidth = 96;
inline constexpr float laneNameSize = 10.0F;
inline constexpr float laneNameSpacing = 0.14F; // em
inline constexpr float lanePatternSize = 10.0F;
inline constexpr int rollX = 166;
inline constexpr int rollPadY = 4;
inline constexpr int rollWidth = 548;
inline constexpr int rollHeight = 56;
inline constexpr float barLineAlpha = 0.18F;
// Note height as a fraction of the roll height, per lane (chords, bass, melody).
inline constexpr std::array<float, 3> noteHeightFraction{0.11F, 0.18F, 0.14F};
inline constexpr int noteMinWidth = 2;
inline constexpr float noteRadius = 1.0F;
inline constexpr int playheadWidth = 1;

inline const juce::Rectangle<int> bottomLine{66, 360, 648, 20};
inline constexpr float bottomLineSize = 11.0F;

// The knob group and its four knobs: the column centres and the face's top edge per row.
inline const juce::Rectangle<int> knobGroup{750, 86, 236, 306};
inline constexpr float knobGroupRadius = 4.0F;
inline constexpr int knobWidth = 118;
inline constexpr float knobDiameter = 56.0F;
inline constexpr std::array<int, 2> knobColumnX{809, 927};
inline constexpr std::array<int, 2> knobFaceY{116, 262};

inline constexpr int keySize = 82; // the sender screen's keys
// The bottom band: the lane group, its three columns and the knobs in them.
inline const juce::Rectangle<int> laneGroup{54, 420, 372, 146};
inline constexpr float laneGroupRadius = 4.0F;
inline constexpr int laneColumnX = 54;
inline constexpr int laneColumnWidth = 124;
inline constexpr int laneLegendCentreY = 436;
inline constexpr float laneLegendSize = 10.0F;
inline constexpr float laneLegendSpacing = 0.12F; // em
inline constexpr int levelKnobOffsetX = 92; // the level knob's centre from its column's left edge
inline constexpr int bandKnobCentreY = 488;
inline constexpr int levelKnobWidth = 48;
inline constexpr int levelKnobHeight = 70;
inline constexpr float levelKnobDiameter = 34.0F;
// The Phase 5 pattern knob slot, left empty: centre offset from the column's left edge, and size.
inline constexpr int patternKnobOffsetX = 38;
inline constexpr float patternKnobDiameter = 44.0F;
inline constexpr int volumeKnobCentreX = 950;
inline constexpr int volumeKnobCentreY = 472;
inline constexpr int volumeKnobWidth = 96;
inline constexpr int volumeKnobHeight = 94;
inline constexpr float volumeKnobDiameter = 52.0F;

// The keys: play and stop, then the sprinkle key. The Phase 5 slots (pin 538, reroll 586, mute
// 634, swap 682, keep 722 and bottle it 770, all at y 464) are left empty.
inline const juce::Rectangle<int> playKey{450, 464, 36, 24};
inline const juce::Rectangle<int> stopKey{498, 464, 36, 24};
inline const juce::Rectangle<int> sprinkleKey{818, 454, 84, 44};
inline constexpr float keyRadius = 3.0F;
inline constexpr float sprinkleRadius = 5.0F;
inline constexpr float keyPressDrop = 1.0F;
inline constexpr float keyShadowDrop = 2.0F; // the hard shadow's offset at rest
inline constexpr float sprinkleShadowDrop = 3.0F;
inline constexpr float sprinkleTitleSize = 10.0F;
inline constexpr float sprinkleTitleSpacing = 0.18F; // em
inline constexpr float ledSize = 6.0F;
inline constexpr float keyLedGap = 6.0F; // from the LED's lower edge to the key's top
inline constexpr float ledGlow = 6.0F;
inline constexpr float keyLegendGap = 8.0F; // from the key's bottom to the legend's top
inline constexpr float keyLegendOverhang = 16.0F;
inline constexpr int keyRepaintMargin = 24; // above and below a key, for its LED, shadow and legend
// The sender screen (visual spec, "Sender screen").
inline constexpr int senderWidth = 520;
inline constexpr int senderHeight = 240;
inline constexpr int senderKeyX = 27;
inline constexpr int senderKeyY = 100;
inline constexpr int senderKeyPitch = 94; // a key's width plus the 12 px gap
inline constexpr int senderMainKeyX = 321;
inline constexpr int senderMainKeyWidth = 120;
inline const juce::Rectangle<int> senderTitle{27, 27, 466, 29};
inline const juce::Rectangle<int> senderStatus{27, 200, 466, 16};
inline constexpr float focusRingOffset = 3.0F;
inline constexpr float focusRingStroke = 2.0F; // the screen's focus outline, drawn just inside
} // namespace layout

} // namespace md::tokens
