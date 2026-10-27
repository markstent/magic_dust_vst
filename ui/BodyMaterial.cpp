#include "ui/BodyMaterial.h"

#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

// Repeating from the bottom edge: 1 px white, then 2 px black.
void paintBrushing(juce::Graphics& g, juce::Rectangle<float> inner)
{
    const auto left = static_cast<int>(inner.getX());
    const auto width = static_cast<int>(inner.getWidth());
    for (int bottom = static_cast<int>(inner.getBottom()); bottom > 0; bottom -= t::brushPeriod)
    {
        g.setColour(juce::Colours::white.withAlpha(t::brushWhiteAlpha));
        g.fillRect(left, bottom - 1, width, 1);
        g.setColour(juce::Colours::black.withAlpha(t::brushBlackAlpha));
        g.fillRect(left, bottom - t::brushPeriod, width, t::brushPeriod - 1);
    }
}

// An inset 1 px white line along the top edge: the padding box minus itself moved down 1 px.
void paintHighlight(juce::Graphics& g, juce::Rectangle<float> inner, float radius)
{
    juce::Path crescents;
    crescents.setUsingNonZeroWinding(false);
    crescents.addRoundedRectangle(inner, radius);
    crescents.addRoundedRectangle(inner.translated(0.0F, 1.0F), radius);
    g.setColour(juce::Colours::white.withAlpha(t::highlightAlpha));
    g.fillPath(crescents);
}

} // namespace

void paintBody(juce::Graphics& g, juce::Rectangle<float> outer)
{
    const auto inner = outer.reduced(t::bodyBorder);
    const auto radius = t::bodyRadius - t::bodyBorder;

    g.setColour(t::bodyEdge);
    g.fillRoundedRectangle(outer, t::bodyRadius);
    g.setColour(t::body);
    g.fillRoundedRectangle(inner, radius);

    juce::Path padding;
    padding.addRoundedRectangle(inner, radius);
    g.saveState();
    g.reduceClipRegion(padding, {});
    paintBrushing(g, inner);
    paintHighlight(g, inner, radius);
    g.restoreState();
}

} // namespace md::ui
