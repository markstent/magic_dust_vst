#include "ui/StatusFields.h"

#include <cmath>

#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr juce::juce_wchar kChevron = 0x203A; // single right-pointing angle quotation mark

void drawLine(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& key, bool chevron,
              const juce::String& value)
{
    const auto font = md::ui::font(Face::monoRegular, t::fieldSize);
    juce::AttributedString line;
    line.setJustification(juce::Justification::centredLeft);
    line.setWordWrap(juce::AttributedString::none);
    line.append(key, font, t::screenQuiet);
    line.append(" " + value, font, t::screenInk);
    if (chevron)
    {
        line.append(" ", font, t::screenInk);
        line.append(juce::String::charToString(kChevron), font, t::gold);
    }
    line.draw(g, area.toFloat());
}

// The aura quadrant: mood and energy each split at 50.
juce::String auraName(juce::AudioProcessorValueTreeState& params)
{
    const auto bright = params.getRawParameterValue("mood")->load() >= t::layout::auraQuadrantSplit;
    const auto wild = params.getRawParameterValue("energy")->load() >= t::layout::auraQuadrantSplit;
    if (wild)
    {
        return bright ? "starburst" : "midnight rave";
    }
    return bright ? "daydream" : "moonlight";
}

} // namespace

FieldRow::FieldRow(juce::AudioProcessorValueTreeState& params, const char* id, bool scrolls)
    : choice_(dynamic_cast<juce::AudioParameterChoice&>(*params.getParameter(id))),
      scrolls_(scrolls)
{
    setComponentID(id);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
}

void FieldRow::refresh()
{
    const auto value = choice_.getCurrentChoiceName();
    if (value != shown_)
    {
        shown_ = value;
        setTitle(choice_.getName(64) + ", " + shown_);
        repaint();
    }
}

void FieldRow::cycle(int steps)
{
    const auto count = choice_.choices.size();
    const auto next = (((choice_.getIndex() + steps) % count) + count) % count;
    choice_.beginChangeGesture();
    choice_.setValueNotifyingHost(choice_.convertTo0to1(static_cast<float>(next)));
    choice_.endChangeGesture();
    refresh();
}

void FieldRow::paint(juce::Graphics& g)
{
    drawLine(g, getLocalBounds(), choice_.getName(64), true, shown_);
    if (hasKeyboardFocus(true))
    {
        drawFocusRing(g, getLocalBounds().toFloat(), t::layout::laneRadius);
    }
}

bool FieldRow::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        cycle(1);
        return true;
    }
    return false;
}

void FieldRow::mouseUp(const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && getLocalBounds().contains(e.getPosition()))
    {
        cycle(1);
    }
}

void FieldRow::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (!scrolls_)
    {
        Component::mouseWheelMove(e, wheel);
        return;
    }
    if (wheel.deltaY != 0.0F)
    {
        cycle(wheel.deltaY < 0.0F ? 1 : -1);
    }
}

StatusFields::StatusFields(juce::AudioProcessorValueTreeState& params)
    : params_(params), groove_(params, "groove", true), flavour_(params, "flavour", false)
{
    addAndMakeVisible(groove_);
    addAndMakeVisible(flavour_);
}

void StatusFields::refresh()
{
    const auto aura = auraName(params_);
    if (aura != aura_)
    {
        aura_ = aura;
        setTitle("aura, " + aura_);
        repaint();
    }
    groove_.refresh();
    flavour_.refresh();
}

void StatusFields::resized()
{
    const auto row = [this](int index)
    {
        return juce::Rectangle<int>(0, index * t::layout::fieldPitch, getWidth(),
                                    t::layout::fieldLine);
    };
    groove_.setBounds(row(1));
    flavour_.setBounds(row(2));
}

void StatusFields::paint(juce::Graphics& g)
{
    drawLine(g, juce::Rectangle<int>(0, 0, getWidth(), t::layout::fieldLine), "aura", false, aura_);
}

} // namespace md::ui
