#pragma once

#include <juce_graphics/juce_graphics.h>

namespace md::ui
{

// Paints the console's metal face, walnut cheeks and four screws filling the bounds.
void paintFaceplate(juce::Graphics& g, juce::Rectangle<float> bounds);

// The header's wordmark and tagline: "MAGIC DUST" from x = 54, then the tagline after a gap, both
// centred on the header line. taglineRight() is the tagline's right edge, which has to stay clear
// of the readout windows.
void paintWordmark(juce::Graphics& g);
int taglineRight();

// Paints a backlit cream window: a bezel border, the cream gradient and an inner glow.
void paintCreamWindow(juce::Graphics& g, juce::Rectangle<float> bounds, float border, float radius,
                      juce::Colour glow, float glowWidth);

} // namespace md::ui
