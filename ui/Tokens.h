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
inline const juce::Colour keyTop{0xFFF4F5F6};
inline const juce::Colour keyBottom{0xFFE3E4E7};
inline const juce::Colour keyEdge{0xFFB4B6BB};
inline const juce::Colour keyBase{0xFFA9ABB0};
inline const juce::Colour knurlLight{0xFFCDCED2};
inline const juce::Colour knurlDark{0xFFA5A7AC};
inline const juce::Colour knurlEdge{0xFF8E9096};
inline const juce::Colour ledOff{0xFFC3C5CA};
inline const juce::Colour cyan{0xFF5FE3F0};
inline const juce::Colour violet{0xFFA68BFF};
inline const juce::Colour violetSoft{0xFFC9B8FF};
inline const juce::Colour violetDeep{0xFF2B1F5C};
inline const juce::Colour magenta{0xFFFF5CA8};
inline const juce::Colour gold{0xFFFFD166};
inline const juce::Colour goldLight{0xFFFFE29A};
inline const juce::Colour goldEdge{0xFFC9992F};
inline const juce::Colour goldBase{0xFFB88A2A};
inline const juce::Colour onColour{0xFF050507};
inline const juce::Colour sprinkleInk{0xFF12101F};
inline const juce::Colour focus{0xFF8C6CF5};

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
inline constexpr float topLineSize = 13.0F;
inline constexpr float bubbleNameSize = 19.0F;
inline constexpr float bubbleNameLongSize = 16.0F;
inline constexpr int bubbleNameLongOver = 5;
inline constexpr float bubbleFeelSize = 10.0F;
inline constexpr float laneLabelSize = 11.0F;
inline constexpr float fieldSize = 13.0F;
inline constexpr float readoutSize = 12.0F;
inline constexpr float statusLineSize = 11.0F;
inline constexpr float keyLabelSize = 10.0F;
inline constexpr float sprinkleTitleSize = 22.0F;
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
inline constexpr int windowHeight = 740;
inline constexpr float minScale = 0.8F;
inline constexpr float maxScale = 1.5F;

inline const juce::Rectangle<int> screenOuter{245, 27, 546, 498};
inline const juce::Rectangle<int> topLine{272, 50, 492, 20};
inline const juce::Rectangle<int> statusLine{27, 547, 986, 16};

inline constexpr int bubbleSize = 92;
inline constexpr int bubbleY = 82;
inline constexpr std::array<int, 4> bubbleX{278, 407, 537, 666};
inline constexpr float bubbleRing = 2.0F;
inline constexpr float playingGap = 4.0F; // the gap between the bubble and its gold ring
inline constexpr float focusRingOffset = 3.0F;
// The most the ring, focus ring or selected dot reach outside the bubble.
inline constexpr int bubbleOverflow = 12;
inline constexpr float selectedDotSize = 6.0F;
inline constexpr float selectedDotGap = 6.0F;
inline constexpr float bubbleLineGap = 2.0F;

inline constexpr int laneX = 272;
inline constexpr int laneWidth = 492;
inline constexpr int laneHeight = 62;
inline constexpr std::array<int, 3> laneY{198, 264, 330};
inline constexpr float laneRadius = 8.0F;
inline constexpr int laneLabelPad = 6;
inline constexpr int rollX = 344;
inline constexpr int rollPadY = 3;
inline constexpr int rollWidth = 414;
inline constexpr int rollHeight = 56;
// Note height as a fraction of the roll height, per lane (chords, bass, melody).
inline constexpr std::array<float, 3> noteHeightFraction{0.11F, 0.18F, 0.14F};
inline constexpr int noteMinWidth = 2;
inline constexpr float noteRadius = 3.0F;
inline constexpr int playheadWidth = 2;

inline const juce::Rectangle<int> auraPad{272, 432, 70, 70};
inline constexpr float auraRadius = 8.0F;
inline constexpr int auraBorder = 1;
inline constexpr float auraStroke = 1.0F;
inline constexpr float auraQuadrantSplit = 50.0F; // mood and energy each split the quadrants here
// The sparkle is drawn on a 24 x 24 grid: a start point, then four cubic curves of three points.
inline constexpr float sparkleGrid = 24.0F;
inline constexpr std::array<float, 2> sparkleStart{12.0F, 2.0F};
inline constexpr std::array<std::array<float, 6>, 4> sparkleCurves{{
    {12.6F, 7.2F, 16.8F, 11.4F, 22.0F, 12.0F},
    {16.8F, 12.6F, 12.6F, 16.8F, 12.0F, 22.0F},
    {11.4F, 16.8F, 7.2F, 12.6F, 2.0F, 12.0F},
    {7.2F, 11.4F, 11.4F, 7.2F, 12.0F, 2.0F},
}};
inline constexpr float auraSparkle = 20.0F;
inline constexpr float auraInset = 0.08F;

inline const juce::Rectangle<int> statusFields{358, 435, 206, 63};
inline constexpr int fieldLine = 19;
inline constexpr int fieldPitch = 22;
inline const juce::Rectangle<int> readout{564, 437, 200, 60};
inline constexpr int readoutLine = 18;
inline constexpr int readoutPitch = 21;

inline constexpr float chevronSize = 12.0F;
inline constexpr float chevronGap = 4.0F;
inline constexpr float chevronGrid = 24.0F; // the chevron is drawn on a 24 x 24 grid
inline constexpr float chevronStroke = 1.0F;
inline constexpr std::array<std::array<float, 2>, 3> chevronPoints{
    {{6.0F, 9.0F}, {12.0F, 15.0F}, {18.0F, 9.0F}}};

inline constexpr int keyY = 575;
inline constexpr int keySize = 82;
inline constexpr int playKeyX = 27;
inline constexpr int stopKeyX = 121;
inline constexpr int sprinkleKeyX = 227;
inline constexpr int sprinkleKeyWidth = 176;
inline constexpr float keyRadius = 12.0F;
inline constexpr float keyPressDrop = 3.0F;
inline constexpr float keyBaseDrop = 4.0F;        // the solid base below a key at rest
inline constexpr float keyBasePressedDrop = 1.0F; // and below a pressed key
inline constexpr float keyShadowDrop = 6.0F;      // the soft shadow's offset at rest
inline constexpr float keyShadowPressedDrop = 2.0F;
inline constexpr float keyShadowPressedBlur = 4.0F;
inline constexpr float keyShadowPressedAlpha = 0.12F;
inline constexpr float keycapShadowBlur = 10.0F;
inline constexpr float keycapShadowAlpha = 0.12F;
inline constexpr float sprinkleShadowBlur = 12.0F;
inline constexpr float sprinkleShadowAlpha = 0.14F;
inline constexpr float keyIconSize = 22.0F;
inline constexpr float stopIconSize = 20.0F;
inline constexpr float keyIconGrid = 24.0F;
inline constexpr float keyLabelGap = 6.0F;
inline constexpr float ledSize = 7.0F;
inline constexpr float ledInset = 10.0F; // from the key's top and right edges
inline constexpr float ledGlow = 6.0F;
inline constexpr float sprinkleIconSize = 28.0F;
inline constexpr float sprinkleIconGap = 10.0F;
inline constexpr float sprinkleLabelGap = 4.0F;
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
inline constexpr float focusRingStroke = 2.0F; // the screen's focus outline, drawn just inside
} // namespace layout

} // namespace md::tokens
