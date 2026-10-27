#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

class ChordBubble : public juce::Button
{
  public:
    explicit ChordBubble(int slot);

    void set(const juce::String& name, const juce::String& feel, bool playing, bool selected);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    int slot_;
    juce::String name_;
    juce::String feel_;
    bool playing_ = false;
    bool selected_ = false;

    MD_COMPONENT_BOILERPLATE(ChordBubble)
};

} // namespace md::ui
