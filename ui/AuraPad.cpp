#include "ui/AuraPad.h"

#include <algorithm>
#include <cmath>

#include "ui/Focus.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr float kSpan = 1.0F - (2.0F * t::layout::auraInset); // the sparkle's travel

// The sparkle's centre as a fraction of the pad's inner box; stays auraInset in from each edge.
float fractionOf(int value)
{
    return t::layout::auraInset + (static_cast<float>(value) / 100.0F * kSpan);
}

int valueOf(float fraction)
{
    const auto v = (fraction - t::layout::auraInset) / kSpan;
    return static_cast<int>(std::lround(std::clamp(v, 0.0F, 1.0F) * 100.0F));
}

} // namespace

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

AuraPad::AuraPad(juce::AudioProcessorValueTreeState& params) : params_(params)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus(true);
}

void AuraPad::set(int mood, int energy)
{
    if (mood == mood_ && energy == energy_)
    {
        return;
    }
    mood_ = mood;
    energy_ = energy;
    setTitle("aura pad, mood " + juce::String(mood_) + ", energy " + juce::String(energy_));
    repaint();
}

void AuraPad::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const auto inner = bounds.reduced(t::layout::auraBorder).toFloat();

    g.setColour(t::screenLine);
    g.drawRoundedRectangle(bounds.toFloat().reduced(t::layout::auraStroke / 2.0F),
                           t::layout::auraRadius, t::layout::auraStroke);
    g.fillRect(bounds.getCentreX(), t::layout::auraBorder, 1,
               bounds.getHeight() - (2 * t::layout::auraBorder));
    g.fillRect(t::layout::auraBorder, bounds.getCentreY(),
               bounds.getWidth() - (2 * t::layout::auraBorder), 1);

    if (hasKeyboardFocus(true))
    {
        drawFocusRing(g, bounds.toFloat(), t::layout::auraRadius);
    }

    const auto centre =
        juce::Point<float>(inner.getX() + (fractionOf(mood_) * inner.getWidth()),
                           inner.getY() + (fractionOf(100 - energy_) * inner.getHeight()));
    const auto scale = t::layout::auraSparkle / t::layout::sparkleGrid;
    g.setColour(t::gold);
    g.fillPath(sparklePath(), juce::AffineTransform::translation(-t::layout::sparkleGrid / 2.0F,
                                                                 -t::layout::sparkleGrid / 2.0F)
                                  .scaled(scale)
                                  .translated(centre.x, centre.y));
}

void AuraPad::mouseDown(const juce::MouseEvent& e)
{
    params_.getParameter("mood")->beginChangeGesture();
    params_.getParameter("energy")->beginChangeGesture();
    move(e.position);
}

void AuraPad::mouseDrag(const juce::MouseEvent& e) { move(e.position); }

void AuraPad::mouseUp(const juce::MouseEvent& /*e*/)
{
    params_.getParameter("mood")->endChangeGesture();
    params_.getParameter("energy")->endChangeGesture();
}

void AuraPad::move(juce::Point<float> position)
{
    const auto inner = getLocalBounds().reduced(t::layout::auraBorder).toFloat();
    const auto mood = valueOf((position.x - inner.getX()) / inner.getWidth());
    const auto energy = 100 - valueOf((position.y - inner.getY()) / inner.getHeight());
    set(mood, energy);
    write("mood", mood);
    write("energy", energy);
}

void AuraPad::write(const char* id, int value)
{
    auto* parameter = params_.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
}

} // namespace md::ui
