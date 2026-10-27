#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

// Dresses the popup menus in the display's cream and ink and the console's mono font.
class MdLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    MdLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, tokens::console::displayCentre);
        setColour(juce::PopupMenu::textColourId, tokens::console::ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, tokens::console::ink);
        setColour(juce::PopupMenu::highlightedTextColourId, tokens::console::displayCentre);
    }

    juce::Font getPopupMenuFont() override { return font(Face::monoRegular, tokens::topLineSize); }
};

} // namespace md::ui
