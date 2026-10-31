#include "ui/ChordCell.h"

#include "ui/Drag.h"
#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

ChordCell::ChordCell(int slot) : juce::Button("chord"), slot_(slot) {}

void ChordCell::set(const juce::String& name, const juce::String& feel, bool playing, bool selected)
{
    if (name == name_ && feel == feel_ && playing == playing_ && selected == selected_)
    {
        return;
    }
    name_ = name;
    feel_ = feel;
    playing_ = playing;
    selected_ = selected;
    setTitle("chord " + juce::String(slot_ + 1) + ", " + name_ + ", " + feel_);
    repaint();
}

bool ChordCell::keyPressed(const juce::KeyPress& key)
{
    // juce::Button answers Enter but not Space.
    if (!isActivateKey(key))
    {
        return false;
    }
    if (onClick)
    {
        onClick();
    }
    return true;
}

void ChordCell::mouseDown(const juce::MouseEvent& e)
{
    // The OS drag can swallow the mouse-up, so every press starts afresh.
    dragging_ = false;
    juce::Button::mouseDown(e);
}

void ChordCell::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging_)
    {
        return;
    }
    juce::Button::mouseDrag(e);
    if (exceedsDragThreshold(e.getMouseDownPosition(), e.getPosition()))
    {
        dragging_ = true;
        // A drag is not a click.
        setState(juce::Button::buttonNormal);
        if (onDrag_)
        {
            onDrag_();
        }
    }
}

void ChordCell::mouseUp(const juce::MouseEvent& e)
{
    if (dragging_)
    {
        dragging_ = false;
        return;
    }
    juce::Button::mouseUp(e);
}

void ChordCell::paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/)
{
    const auto cell = getLocalBounds().toFloat().reduced(t::layout::cellInset);
    const auto ink = t::console::ink;

    const auto nameFont = font(Face::monoBold, t::layout::cellNameSize);
    const auto feelFont = font(Face::monoRegular, t::layout::cellFeelSize);
    const auto nameHeight = nameFont.getHeight();
    const auto feelHeight = feelFont.getHeight();
    const auto top = cell.getY() + ((cell.getHeight() - (nameHeight + feelHeight)) / 2.0F) -
                     t::layout::cellTextLift;
    g.setFont(nameFont);
    g.setColour(ink);
    g.drawText(name_, juce::Rectangle<float>(cell.getX(), top, cell.getWidth(), nameHeight),
               juce::Justification::centred, true);
    g.setFont(feelFont);
    g.setColour(ink.withAlpha(t::layout::cellFeelAlpha));
    g.drawText(feel_,
               juce::Rectangle<float>(cell.getX(), top + nameHeight, cell.getWidth(), feelHeight),
               juce::Justification::centred, true);

    g.setColour(ink);
    if (playing_)
    {
        g.fillRect(juce::Rectangle<float>(t::layout::cellBarWidth, t::layout::cellBarHeight)
                       .withCentre({cell.getCentreX(),
                                    cell.getBottom() - (t::layout::cellBarHeight / 2.0F) + 1.0F}));
    }
    if (selected_)
    {
        g.drawRoundedRectangle(cell.reduced(t::layout::cellOutline / 2.0F), t::layout::cellRadius,
                               t::layout::cellOutline);
    }
    if (hasKeyboardFocus(true))
    {
        drawFocusRing(g, cell.expanded(t::layout::focusRingStroke), t::layout::cellRadius, ink);
    }
}

} // namespace md::ui
