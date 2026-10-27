#include "ui/ChordBubble.h"

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

ChordBubble::ChordBubble(int slot) : juce::Button("chord"), slot_(slot)
{
    // The ring, gap and dot are drawn outside the 92 px bounds.
    setPaintingIsUnclipped(true);
}

void ChordBubble::set(const juce::String& name, const juce::String& feel, bool playing,
                      bool selected)
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
    repaint(getLocalBounds().expanded(12, 14));
}

void ChordBubble::paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour(t::violet);
    g.drawEllipse(bounds.reduced(t::layout::bubbleRing / 2.0F), t::layout::bubbleRing);

    if (playing_)
    {
        g.setColour(t::gold);
        g.drawEllipse(bounds.expanded(t::layout::playingRingOffset - 1.0F), 2.0F);
    }
    if (hasKeyboardFocus(true))
    {
        g.setColour(t::gold);
        g.drawEllipse(bounds.expanded(t::layout::focusRingOffset + 1.0F), 2.0F);
    }
    if (selected_)
    {
        const auto size = t::layout::selectedDotSize;
        g.setColour(t::screenInk);
        g.fillEllipse(bounds.getCentreX() - (size / 2.0F),
                      bounds.getBottom() + t::layout::selectedDotGap, size, size);
    }

    const auto nameFont =
        font(Face::groteskBold,
             name_.length() > t::bubbleNameLongOver ? t::bubbleNameLongSize : t::bubbleNameSize);
    const auto feelFont = font(Face::monoRegular, t::bubbleFeelSize);
    const auto nameHeight = nameFont.getHeight();
    const auto feelHeight = feelFont.getHeight();
    const auto top =
        (bounds.getHeight() - (nameHeight + t::layout::bubbleLineGap + feelHeight)) / 2.0F;

    g.setFont(nameFont);
    g.setColour(t::screenInk);
    g.drawText(name_, juce::Rectangle<float>(0.0F, top, bounds.getWidth(), nameHeight),
               juce::Justification::centred, false);
    g.setFont(feelFont);
    g.setColour(t::violetSoft);
    g.drawText(feel_,
               juce::Rectangle<float>(0.0F, top + nameHeight + t::layout::bubbleLineGap,
                                      bounds.getWidth(), feelHeight),
               juce::Justification::centred, false);
}

} // namespace md::ui
