#include "ui/Knob.h"

#include <algorithm>
#include <cmath>

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace
{

constexpr float kStartAngle = -135.0F; // degrees from straight up, clockwise
constexpr float kSweep = 270.0F;
constexpr float kLegendHeight = 14.0F;
constexpr float kContinuousMargin = 12.0F; // room for the ticks above and beside the face
constexpr float kSteppedMargin = 21.0F;    // room for the ticks and their step words
constexpr float kTickGap = 5.0F;
constexpr float kTickLength = 4.0F;
constexpr float kWordGap = 8.0F;
constexpr float kWordWidth = 56.0F;
constexpr float kWordHeight = 11.0F;
constexpr float kWordSize = 8.0F;
constexpr float kLegendSize = 10.0F;
constexpr float kLegendSpacing = 1.2F;
constexpr int kContinuousTicks = 16;
constexpr float kCapRatio = 0.52F;
constexpr float kFocusStroke = 2.0F;
constexpr float kPixelsPerStep = 16.0F;
constexpr double kFullRangePixels = 160.0;
constexpr double kShiftFactor = 4.0;
constexpr double kHundredWheelStep = 5.0;
constexpr double kUnitWheelStep = 0.05;
constexpr float kWheelNotch = 0.125F; // deltaY of one mouse wheel notch on macOS
constexpr float kWheelTolerance = 0.001F;

juce::String textForValue(juce::RangedAudioParameter* parameter, double value)
{
    if (const auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter))
    {
        return choice->choices.getReference(
            juce::jlimit(0, choice->choices.size() - 1, static_cast<int>(std::lround(value))));
    }
    return parameter->getText(parameter->convertTo0to1(static_cast<float>(value)), 0);
}

} // namespace

Knob::Knob(juce::AudioProcessorValueTreeState& params, const juce::String& paramId, KnobLook look)
    : look_(std::move(look)),
      stepped_(dynamic_cast<juce::AudioParameterChoice*>(params.getParameter(paramId)) != nullptr),
      attachment_(*params.getParameter(paramId), *this)
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(juce::degreesToRadians(kStartAngle + 360.0F),
                        juce::degreesToRadians(kStartAngle + kSweep + 360.0F), true);
    jassert(look_.stepWords.isEmpty() ||
            look_.stepWords.size() == static_cast<int>(getMaximum() - getMinimum()) + 1);
    setTitle(look_.legend.toLowerCase());
    setWantsKeyboardFocus(true);
    auto* parameter = params.getParameter(paramId);
    textFromValueFunction = [parameter](double value) { return textForValue(parameter, value); };
    const auto pixels = stepped_
                            ? static_cast<double>(kPixelsPerStep) * (getMaximum() - getMinimum())
                            : kFullRangePixels;
    setMouseDragSensitivity(static_cast<int>(pixels));
    if (look_.resetValue.has_value())
    {
        setDoubleClickReturnValue(true, *look_.resetValue);
    }
}

double Knob::step() const
{
    if (stepped_)
    {
        return 1.0;
    }
    return getMaximum() > 2.0 ? kHundredWheelStep : kUnitWheelStep;
}

juce::Rectangle<int> Knob::faceBounds() const
{
    const auto margin = look_.stepWords.isEmpty() ? kContinuousMargin : kSteppedMargin;
    const auto size = static_cast<int>(look_.diameter);
    return {(getWidth() - size) / 2, static_cast<int>(kLegendHeight + margin + look_.legendGap),
            size, size};
}

void Knob::mouseDown(const juce::MouseEvent& e)
{
    anchor(e);
    juce::Slider::mouseDown(e);
}

// The drag measures from here: the value and pointer height now, and whether it is fine.
void Knob::anchor(const juce::MouseEvent& e)
{
    fine_ = e.mods.isShiftDown();
    anchorProportion_ = valueToProportionOfLength(getValue());
    anchorUp_ = -static_cast<double>(e.getDistanceFromDragStartY());
}

// Shift-drag is 4 times finer than a plain drag. Pressing or releasing shift mid-drag starts the
// measuring afresh, so the value never jumps.
void Knob::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isShiftDown() != fine_)
    {
        anchor(e);
    }
    const auto up = -static_cast<double>(e.getDistanceFromDragStartY()) - anchorUp_;
    const auto pixels =
        static_cast<double>(getMouseDragSensitivity()) * (fine_ ? kShiftFactor : 1.0);
    setValue(proportionOfLengthToValue(juce::jlimit(0.0, 1.0, anchorProportion_ + (up / pixels))),
             juce::sendNotificationSync);
}

// A smooth trackpad swipe sends many small deltas, so they add up to one step per mouse notch.
void Knob::mouseWheelMove(const juce::MouseEvent& /*e*/, const juce::MouseWheelDetails& wheel)
{
    const auto delta = wheel.deltaY != 0.0F ? wheel.deltaY : -wheel.deltaX;
    wheelAccumulated_ += wheel.isReversed ? -delta : delta;
    const auto notches = std::trunc((wheelAccumulated_ / kWheelNotch) + kWheelTolerance);
    if (notches != 0.0F)
    {
        wheelAccumulated_ -= notches * kWheelNotch;
        setValue(getValue() + (static_cast<double>(notches) * step()), juce::sendNotificationSync);
    }
}

void Knob::paint(juce::Graphics& g)
{
    const auto face = faceBounds().toFloat();
    const auto centre = face.getCentre();
    const auto faceRadius = face.getWidth() / 2.0F;

    g.setColour(tokens::console::legend);
    g.setFont(
        font(Face::groteskBold, kLegendSize).withExtraKerningFactor(kLegendSpacing / kLegendSize));
    g.drawText(look_.legend, getLocalBounds().removeFromTop(static_cast<int>(kLegendHeight)),
               juce::Justification::centred, false);

    if (look_.subLegend.isNotEmpty())
    {
        g.setFont(font(Face::groteskMedium, kWordSize));
        g.drawText(look_.subLegend,
                   juce::Rectangle<float>(0.0F, kLegendHeight, static_cast<float>(getWidth()),
                                          kWordHeight),
                   juce::Justification::centred, false);
    }

    const int ticks =
        stepped_ ? static_cast<int>(getMaximum() - getMinimum()) + 1 : kContinuousTicks;
    g.setFont(font(Face::groteskMedium, kWordSize));
    for (int i = 0; i < ticks; ++i)
    {
        const auto angle = kStartAngle + (kSweep * static_cast<float>(i) /
                                          static_cast<float>(std::max(1, ticks - 1)));
        const auto theta = juce::degreesToRadians(angle);
        const auto tickStart = faceRadius + kTickGap;
        g.drawLine({centre.getPointOnCircumference(tickStart, theta),
                    centre.getPointOnCircumference(tickStart + kTickLength, theta)},
                   1.0F);
        if (!look_.stepWords.isEmpty())
        {
            const auto at =
                centre.getPointOnCircumference(tickStart + kTickLength + kWordGap, theta);
            g.drawText(look_.stepWords.getReference(i),
                       juce::Rectangle<float>(kWordWidth, kWordHeight).withCentre(at),
                       juce::Justification::centred, false);
        }
    }

    juce::Path shadow;
    shadow.addEllipse(face);
    juce::DropShadow(juce::Colours::black.withAlpha(0.5F), 5, {0, 2}).drawForPath(g, shadow);
    g.setGradientFill(juce::ColourGradient(
        tokens::console::knobLight, centre.translated(-faceRadius / 3.0F, -faceRadius / 3.0F),
        tokens::console::knobDark, centre.translated(faceRadius, faceRadius), true));
    g.fillEllipse(face);

    const auto cap =
        face.withSizeKeepingCentre(face.getWidth() * kCapRatio, face.getHeight() * kCapRatio);
    g.setColour(look_.cap);
    g.fillEllipse(cap);

    const auto proportion = static_cast<float>(valueToProportionOfLength(getValue()));
    const auto angle = kStartAngle + (kSweep * proportion);
    const auto theta = juce::degreesToRadians(angle);
    g.setColour(tokens::console::legend);
    g.drawLine({centre.getPointOnCircumference(faceRadius * kCapRatio / 2.0F * 0.35F, theta),
                centre.getPointOnCircumference(faceRadius * kCapRatio / 2.0F, theta)},
               2.5F);

    if (look_.rangeWords.size() == 2)
    {
        g.setColour(tokens::console::legend);
        g.setFont(font(Face::groteskMedium, kWordSize));
        // Each word's inner edge sits under the end of the scale and the word runs outward.
        const auto y = face.getBottom() + kTickGap;
        const auto scaleEnd =
            (faceRadius + kTickGap + kTickLength) * std::sin(juce::degreesToRadians(45.0F));
        g.drawText(
            look_.rangeWords.getReference(0),
            juce::Rectangle<float>(centre.x - scaleEnd - kWordWidth, y, kWordWidth, kWordHeight),
            juce::Justification::centredRight, false);
        g.drawText(look_.rangeWords.getReference(1),
                   juce::Rectangle<float>(centre.x + scaleEnd, y, kWordWidth, kWordHeight),
                   juce::Justification::centredLeft, false);
    }

    if (hasKeyboardFocus(false))
    {
        g.setColour(tokens::console::legend);
        g.drawEllipse(face.expanded(kTickGap - kFocusStroke), kFocusStroke);
    }
}

} // namespace md::ui
