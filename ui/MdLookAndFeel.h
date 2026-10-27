#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

// Dresses the popup menus in the screen's colours and font.
class MdLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    MdLookAndFeel()
    {
        setColour(juce::PopupMenu::backgroundColourId, tokens::screenPanel);
        setColour(juce::PopupMenu::textColourId, tokens::screenInk);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, tokens::spellCurrent);
        setColour(juce::PopupMenu::highlightedTextColourId, tokens::gold);
    }

    juce::Font getPopupMenuFont() override { return font(Face::monoRegular, tokens::topLineSize); }
};

} // namespace md::ui
