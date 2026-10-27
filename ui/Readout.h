#pragma once

#include <optional>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

// The one line along the bottom of the display: bar, selection and pins, or an error.
class Readout : public juce::Component
{
  public:
    Readout() = default;

    // bar is the 1-based bar being played, or 0 when stopped.
    void set(int bar, int bars, const juce::String& selected, const juce::String& pinned);
    // An error replaces the readout until it is cleared.
    void setError(std::optional<std::string> error);
    [[nodiscard]] const juce::String& getText() const { return text_; }
    void paint(juce::Graphics& g) override;

  private:
    void update();

    juce::String readout_;
    std::optional<std::string> error_;
    juce::String text_;

    MD_COMPONENT_BOILERPLATE(Readout)
};

} // namespace md::ui
