#include "ui/SprinkleKey.h"

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

const KeyMaterial sprinkle{.top = t::console::sprinkleTop,
                           .bottom = t::console::sprinkleBottom,
                           .radius = t::layout::sprinkleRadius,
                           .shadowDrop = t::layout::sprinkleShadowDrop};

constexpr const char* kTitle = "sprinkle";

} // namespace

SprinkleKey::SprinkleKey() : KeyButton(kTitle, sprinkle, t::console::ledOn)
{
    setLegendBelow(false);
}

void SprinkleKey::paintContent(juce::Graphics& g, juce::Rectangle<float> face) const
{
    g.setColour(juce::Colours::white);
    g.setFont(font(Face::groteskBold, t::layout::sprinkleTitleSize)
                  .withExtraKerningFactor(t::layout::sprinkleTitleSpacing));
    g.drawText(juce::String(kTitle).toUpperCase(), face, juce::Justification::centred, false);
}

} // namespace md::ui
