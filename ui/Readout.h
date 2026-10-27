#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

class Readout : public juce::Component
{
  public:
    Readout() = default;

    // bar is the 1-based bar being played, or 0 when stopped.
    void set(int bar, int bars, const juce::String& selected, const juce::String& pinned);
    void paint(juce::Graphics& g) override;

  private:
    int bar_ = 0;
    int bars_ = 0;
    juce::String selected_;
    juce::String pinned_;

    MD_COMPONENT_BOILERPLATE(Readout)
};

} // namespace md::ui
