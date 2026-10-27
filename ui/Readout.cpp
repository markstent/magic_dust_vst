#include "ui/Readout.h"

#include <utility>

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

void Readout::set(int bar, int bars, const juce::String& selected, const juce::String& pinned)
{
    const auto position = bar > 0 ? "bar " + juce::String(bar) + " of " + juce::String(bars)
                                  : juce::String("stopped");
    readout_ = (position + " - " + selected + " - " + pinned).toUpperCase();
    update();
}

void Readout::setError(std::optional<std::string> error)
{
    error_ = std::move(error);
    update();
}

void Readout::update()
{
    const auto text = error_ ? juce::String(*error_) : readout_;
    if (text != text_)
    {
        text_ = text;
        repaint();
    }
}

juce::String Readout::drawnText() const
{
    return ellipsize(font(Face::monoRegular, t::layout::bottomLineSize), text_,
                     static_cast<float>(getWidth()));
}

void Readout::paint(juce::Graphics& g)
{
    g.setFont(font(Face::monoRegular, t::layout::bottomLineSize));
    g.setColour(t::console::ink);
    g.drawText(drawnText(), getLocalBounds(), juce::Justification::centredLeft, false);
}

} // namespace md::ui
