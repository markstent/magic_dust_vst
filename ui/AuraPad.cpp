#include "ui/AuraPad.h"

#include <algorithm>
#include <cmath>

#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr int kBorder = 1;
constexpr float kSpan = 1.0F - (2.0F * t::layout::auraInset); // the sparkle's travel
constexpr float kSparkleView = 24.0F; // the sparkle path is drawn on a 24 x 24 grid

juce::Path sparkle()
{
    juce::Path p;
    p.startNewSubPath(12, 2);
    p.cubicTo(12.6F, 7.2F, 16.8F, 11.4F, 22, 12);
    p.cubicTo(16.8F, 12.6F, 12.6F, 16.8F, 12, 22);
    p.cubicTo(11.4F, 16.8F, 7.2F, 12.6F, 2, 12);
    p.cubicTo(7.2F, 11.4F, 11.4F, 7.2F, 12, 2);
    p.closeSubPath();
    return p;
}

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

AuraPad::AuraPad(juce::AudioProcessorValueTreeState& params) : params_(params)
{
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
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
    const auto inner = bounds.reduced(kBorder).toFloat();

    g.setColour(t::screenLine);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5F), t::layout::auraRadius, 1.0F);
    g.fillRect(bounds.getCentreX(), kBorder, 1, bounds.getHeight() - (2 * kBorder));
    g.fillRect(kBorder, bounds.getCentreY(), bounds.getWidth() - (2 * kBorder), 1);

    const auto centre =
        juce::Point<float>(inner.getX() + (fractionOf(mood_) * inner.getWidth()),
                           inner.getY() + (fractionOf(100 - energy_) * inner.getHeight()));
    const auto scale = t::layout::auraSparkle / kSparkleView;
    g.setColour(t::gold);
    g.fillPath(sparkle(),
               juce::AffineTransform::translation(-kSparkleView / 2.0F, -kSparkleView / 2.0F)
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
    const auto inner = getLocalBounds().reduced(kBorder).toFloat();
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
