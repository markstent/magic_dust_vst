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
inline const juce::Colour focus{0xFF8C6CF5};

// Body material.
inline constexpr float bodyRadius = 28.0F;
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
inline constexpr float playingRingOffset = 6.0F; // 4 px gap then the 2 px ring
inline constexpr float focusRingOffset = 3.0F;
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
} // namespace layout

} // namespace md::tokens
