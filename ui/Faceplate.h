#pragma once

#include <juce_graphics/juce_graphics.h>

namespace md::ui
{

// Paints the console's metal face, walnut cheeks and four screws filling the bounds.
void paintFaceplate(juce::Graphics& g, juce::Rectangle<float> bounds);

// Paints a backlit cream window: a bezel border, the cream gradient and an inner glow.
void paintCreamWindow(juce::Graphics& g, juce::Rectangle<float> bounds, float border, float radius,
                      juce::Colour glow, float glowWidth);

} // namespace md::ui
