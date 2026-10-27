#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "md/types.hpp"

class MagicDustProcessor;

#include "ui/Boilerplate.h"

namespace md::ui
{

// "a min" for aeolian, "a maj" for ionian, else the mode name ("a dorian").
juce::String keyText(const md::Key& key);

// Quiet text that opens a menu when clicked.
class MenuText : public juce::Button
{
  public:
    explicit MenuText(const juce::String& id);

    void setText(const juce::String& text);
    [[nodiscard]] const juce::String& getText() const { return text_; }
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    juce::String text_;

    MD_COMPONENT_BOILERPLATE(MenuText)
};

// Spell number on the left; key, tempo and style on the right. The component IDs "keyMenu" and
// "styleMenu" name the two menu buttons.
class TopLine : public juce::Component
{
  public:
    explicit TopLine(MagicDustProcessor& processor);

    void refresh();
    void paint(juce::Graphics& g) override;

  private:
    void openKeyMenu();
    void openStyleMenu();
    void layoutRight();

    MagicDustProcessor& processor_;
    MenuText key_;
    MenuText style_;
    juce::String spell_;
    juce::String tempo_;
    juce::Rectangle<int> tempoArea_;

    MD_COMPONENT_BOILERPLATE(TopLine)
};

} // namespace md::ui
