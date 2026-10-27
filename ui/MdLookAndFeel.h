#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

// Dresses the popup menus in the screen's colours and font, and draws the lane level sliders.
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

    // A thin rounded track filled to the value in the track colour, with no thumb.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): JUCE's signature.
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        const auto track =
            bounds.withSizeKeepingCentre(bounds.getWidth(), tokens::layout::laneLevelTrackHeight);
        const auto radius = track.getHeight() / 2.0F;
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.fillRoundedRectangle(track, radius);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRoundedRectangle(track.withRight(sliderPos), radius);
        if (slider.hasKeyboardFocus(false))
        {
            drawFocusRing(g, slider.getLocalBounds().toFloat(), bounds.getHeight() / 2.0F);
        }
    }

    juce::Font getPopupMenuFont() override { return font(Face::monoRegular, tokens::topLineSize); }
};

} // namespace md::ui
