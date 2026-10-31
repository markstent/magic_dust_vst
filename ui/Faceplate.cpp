#include "ui/Faceplate.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;
namespace c = md::tokens::console;

namespace
{

// Vertical walnut stripes, repeating a, b, c across the cheek, with a shadow on the face side.
void paintCheek(juce::Graphics& g, juce::Rectangle<float> cheek, bool faceOnRight)
{
    const std::array<juce::Colour, 3> colours{c::walnutA, c::walnutB, c::walnutC};
    float x = cheek.getX();
    std::size_t stripe = 0;
    while (x < cheek.getRight())
    {
        const auto width = static_cast<float>(t::layout::walnutStripes.at(stripe % 3));
        g.setColour(colours.at(stripe % 3));
        g.fillRect(x, cheek.getY(), std::min(width, cheek.getRight() - x), cheek.getHeight());
        x += width;
        ++stripe;
    }
    const auto width = t::layout::cheekShadowWidth;
    const auto edge = faceOnRight ? cheek.getRight() : cheek.getX();
    const auto inner = faceOnRight ? edge - width : edge + width;
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.5F), edge, 0.0F,
                                           juce::Colours::transparentBlack, inner, 0.0F, false));
    g.fillRect(std::min(edge, inner), cheek.getY(), width, cheek.getHeight());
}

void paintScrew(juce::Graphics& g, juce::Point<float> topLeft)
{
    const auto size = t::layout::screwSize;
    const juce::Rectangle<float> screw(topLeft.x, topLeft.y, size, size);
    const auto lit = topLeft.translated(size * 0.35F, size * 0.3F);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFE2E2E2), lit, juce::Colour(0xFF6D6D6D),
                                           lit.translated(size * 0.7F, 0.0F), true));
    g.fillEllipse(screw);

    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::rotation(
        juce::degreesToRadians(t::layout::screwSlotAngle), screw.getCentreX(), screw.getCentreY()));
    g.setColour(juce::Colour(0xFF444444));
    g.fillRect(juce::Rectangle<float>(size - 4.0F, t::layout::screwSlotWidth)
                   .withCentre(screw.getCentre()));
}

} // namespace

void paintFaceplate(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto cheekWidth = static_cast<float>(t::layout::cheekWidth);
    auto face = bounds;
    const auto left = face.removeFromLeft(cheekWidth);
    const auto right = face.removeFromRight(cheekWidth);

    g.setGradientFill(juce::ColourGradient(c::faceTop, 0.0F, face.getY(), c::faceBottom, 0.0F,
                                           face.getBottom(), false));
    g.fillRect(face);
    paintCheek(g, left, true);
    paintCheek(g, right, false);

    const auto inset = t::layout::screwInset;
    const auto size = t::layout::screwSize;
    for (const auto x : {face.getX() + inset, face.getRight() - inset - size})
    {
        for (const auto y : {face.getY() + inset, face.getBottom() - inset - size})
        {
            paintScrew(g, {x, y});
        }
    }
}

namespace
{

constexpr const char* kWordmark = "MAGIC DUST";
constexpr const char* kTagline = "HOUSE MUSIC IDEA ENGINE";
constexpr int kTextSlack = 4; // room for the last letter's spacing
constexpr int kLineHeight = 32;

juce::Font wordmarkFont()
{
    return font(Face::serifBold, t::layout::wordmarkFaceSize)
        .withExtraKerningFactor(t::layout::wordmarkFaceSpacing);
}

juce::Font taglineFont()
{
    return font(Face::groteskMedium, t::layout::taglineSize)
        .withExtraKerningFactor(t::layout::taglineSpacing);
}

int taglineLeft()
{
    return t::layout::wordmarkX +
           juce::GlyphArrangement::getStringWidthInt(wordmarkFont(), kWordmark) +
           t::layout::taglineGap;
}

} // namespace

int taglineRight()
{
    return taglineLeft() + juce::GlyphArrangement::getStringWidthInt(taglineFont(), kTagline);
}

namespace
{

juce::Rectangle<int> headerLine()
{
    return {0, t::layout::headerCentreY - (kLineHeight / 2), 0, kLineHeight};
}

} // namespace

juce::Rectangle<int> wordmarkBounds()
{
    return headerLine()
        .withX(t::layout::wordmarkX)
        .withWidth(juce::GlyphArrangement::getStringWidthInt(wordmarkFont(), kWordmark) +
                   kTextSlack);
}

void paintWordmark(juce::Graphics& g)
{
    const auto line = headerLine();
    g.setFont(wordmarkFont());
    g.setColour(c::legend);
    g.drawText(kWordmark, wordmarkBounds(), juce::Justification::centredLeft, false);

    const auto tagline = taglineFont();
    g.setFont(tagline);
    g.setColour(c::tagline);
    g.drawText(
        kTagline,
        line.withX(taglineLeft())
            .withWidth(juce::GlyphArrangement::getStringWidthInt(tagline, kTagline) + kTextSlack),
        juce::Justification::centredLeft, false);
}

void paintCreamWindow(juce::Graphics& g, juce::Rectangle<float> bounds, float border, float radius,
                      juce::Colour glow, float glowWidth)
{
    g.setColour(c::bezel);
    g.fillRoundedRectangle(bounds, radius);

    const auto inner = bounds.reduced(border);
    const auto innerRadius = std::max(radius - border, 1.0F);
    const juce::Point<float> centre(inner.getCentreX(), inner.getY() + (inner.getHeight() * 0.4F));
    g.setGradientFill(juce::ColourGradient(
        c::displayCentre, centre, c::displayEdge,
        centre.translated(std::hypot(inner.getWidth(), inner.getHeight()) * 0.55F, 0.0F), true));
    g.fillRoundedRectangle(inner, innerRadius);

    // The glow is a stack of strokes along the inner edge, fading towards the middle.
    for (int i = 0; i < static_cast<int>(glowWidth); ++i)
    {
        const auto inset = static_cast<float>(i);
        const auto fade = 1.0F - (inset / glowWidth);
        g.setColour(glow.withMultipliedAlpha(fade * fade));
        g.drawRoundedRectangle(inner.reduced(inset + 0.5F), std::max(innerRadius - inset, 0.5F),
                               1.0F);
    }
}

} // namespace md::ui
