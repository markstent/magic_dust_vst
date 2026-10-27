#include "ui/Readout.h"

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

void Readout::set(int bar, int bars, const juce::String& selected, const juce::String& pinned)
{
    if (bar == bar_ && bars == bars_ && selected == selected_ && pinned == pinned_)
    {
        return;
    }
    bar_ = bar;
    bars_ = bars;
    selected_ = selected;
    pinned_ = pinned;
    repaint();
}

void Readout::paint(juce::Graphics& g)
{
    g.setFont(font(Face::monoRegular, t::readoutSize));
    const auto line = [this, &g](int index, const juce::String& text, juce::Colour colour)
    {
        g.setColour(colour);
        g.drawText(text,
                   juce::Rectangle<int>(0, index * t::layout::readoutPitch, getWidth(),
                                        t::layout::readoutLine),
                   juce::Justification::centredRight, false);
    };
    if (bar_ > 0)
    {
        line(0, "bar " + juce::String(bar_) + " of " + juce::String(bars_), t::gold);
    }
    else
    {
        line(0, "stopped", t::screenQuiet);
    }
    line(1, selected_, t::screenQuiet);
    line(2, pinned_, t::screenQuiet);
}

} // namespace md::ui
