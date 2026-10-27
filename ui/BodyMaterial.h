#pragma once

#include <juce_graphics/juce_graphics.h>

namespace md::ui
{

// Paints the instrument body (visual spec, Materials) filling the bounds.
void paintBody(juce::Graphics& g, juce::Rectangle<float> outer);

} // namespace md::ui
