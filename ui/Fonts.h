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
    monoBold
};

// size is the CSS font size in px.
juce::Font font(Face face, float size);

} // namespace md::ui
