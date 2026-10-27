#include "ui/SprinkleKey.h"

#include <algorithm>

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

const KeyMaterial gold{.top = t::goldLight,
                       .bottom = t::gold,
                       .edge = t::goldEdge,
                       .base = t::goldBase,
                       .shadowBlur = t::layout::sprinkleShadowBlur,
                       .shadowAlpha = t::layout::sprinkleShadowAlpha};

constexpr const char* kTitle = "sprinkle";
constexpr const char* kSubline = "new idea";

// The four-point sparkle on its 24 x 24 grid.
juce::Path sparklePath()
{
    juce::Path p;
    p.startNewSubPath(t::layout::sparkleStart.at(0), t::layout::sparkleStart.at(1));
    for (const auto& c : t::layout::sparkleCurves)
    {
        p.cubicTo(c.at(0), c.at(1), c.at(2), c.at(3), c.at(4), c.at(5));
    }
    p.closeSubPath();
    return p;
}

} // namespace

SprinkleKey::SprinkleKey() : KeyButton(kTitle, gold, std::nullopt) {}

void SprinkleKey::paintContent(juce::Graphics& g, juce::Rectangle<float> face) const
{
    const auto titleFont = font(Face::groteskBold, t::sprinkleTitleSize);
    const auto sublineFont = font(Face::monoRegular, t::keyLabelSize);
    const auto titleHeight = t::sprinkleTitleSize; // line height 1
    const auto sublineHeight = sublineFont.getHeight();
    const auto stackHeight = titleHeight + t::layout::sprinkleLabelGap + sublineHeight;
    const auto stackWidth = std::max(juce::GlyphArrangement::getStringWidth(titleFont, kTitle),
                                     juce::GlyphArrangement::getStringWidth(sublineFont, kSubline));
    const auto iconSize = t::layout::sprinkleIconSize;
    const auto contentWidth = iconSize + t::layout::sprinkleIconGap + stackWidth;
    const auto left = face.getCentreX() - (contentWidth / 2.0F);
    const auto top = face.getCentreY() - (stackHeight / 2.0F);

    g.setColour(t::sprinkleInk);
    g.fillPath(sparklePath(), juce::AffineTransform::scale(iconSize / t::layout::sparkleGrid)
                                  .translated(left, face.getCentreY() - (iconSize / 2.0F)));

    const auto textLeft = left + iconSize + t::layout::sprinkleIconGap;
    g.setFont(titleFont);
    g.drawText(kTitle, juce::Rectangle<float>(textLeft, top, stackWidth, titleHeight),
               juce::Justification::centredLeft, false);
    g.setFont(sublineFont);
    g.drawText(kSubline,
               juce::Rectangle<float>(textLeft, top + titleHeight + t::layout::sprinkleLabelGap,
                                      stackWidth, sublineHeight),
               juce::Justification::centredLeft, false);
}

} // namespace md::ui
