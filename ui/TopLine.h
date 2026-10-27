#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "md/types.hpp"

class MagicDustProcessor;

#include "ui/Boilerplate.h"

namespace md::ui
{

// "a min" for aeolian, "a maj" for ionian, else the mode name ("a dorian").
juce::String keyText(const md::Key& key);

// A cream window with centred text. An interactive one is a button; the others only display.
class ReadoutWindow : public juce::Button
{
  public:
    ReadoutWindow(const juce::String& id, juce::String legend, bool interactive);

    void setText(const juce::String& text);
    [[nodiscard]] const juce::String& getText() const { return text_; }
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    juce::String legend_;
    juce::String text_;

    MD_COMPONENT_BOILERPLATE(ReadoutWindow)
};

// The header: the spell, style, key and tempo windows, each with its legend above. The component
// IDs "spellReadout", "styleMenu", "keyMenu" and "tempoReadout" name the windows.
class TopLine : public juce::Component
{
  public:
    explicit TopLine(MagicDustProcessor& processor);

    void refresh();
    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    void openKeyMenu();
    void openStyleMenu();

    MagicDustProcessor& processor_;
    ReadoutWindow spell_;
    ReadoutWindow style_;
    ReadoutWindow key_;
    ReadoutWindow tempo_;

    MD_COMPONENT_BOILERPLATE(TopLine)
};

} // namespace md::ui
