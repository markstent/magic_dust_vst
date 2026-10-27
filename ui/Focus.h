#pragma once

#include <juce_graphics/juce_graphics.h>

#include "ui/Tokens.h"

namespace md::ui
{

// Space and Enter activate whatever has the focus.
inline bool isActivateKey(const juce::KeyPress& key)
{
    return key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey;
}

// The 2 px focus outline, drawn just inside the bounds: in the colour given.
inline void drawFocusRing(juce::Graphics& g, juce::Rectangle<float> bounds, float radius,
                          juce::Colour colour)
{
    const auto stroke = tokens::layout::focusRingStroke;
    g.setColour(colour);
    g.drawRoundedRectangle(bounds.reduced(stroke / 2.0F), radius, stroke);
}

} // namespace md::ui
