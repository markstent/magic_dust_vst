#pragma once

#include <cstdint>

#include <juce_graphics/juce_graphics.h>

namespace md::ui
{

enum class Face : std::uint8_t
{
    groteskMedium,
    groteskBold,
    monoRegular,
    monoBold,
    serifBold
};

// size is the CSS font size in px.
juce::Font font(Face face, float size);

// The text cut short with "..." so it is no wider than maxWidth in this font, or unchanged if it
// already fits.
juce::String ellipsize(const juce::Font& font, const juce::String& text, float maxWidth);

} // namespace md::ui
