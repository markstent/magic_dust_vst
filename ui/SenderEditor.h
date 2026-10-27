#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"
#include "ui/KeyButton.h"

class MagicDustProcessor;

namespace md::ui
{

// What a sender instance shows, at its logical 520 x 240 size: pick the lane it sends, or become
// a full Magic Dust.
class SenderBody : public juce::Component
{
  public:
    explicit SenderBody(MagicDustProcessor& processor);

    void refresh();
    [[nodiscard]] const juce::String& status() const { return status_; }
    // The status as drawn: cut short with an ellipsis to fit its window.
    [[nodiscard]] juce::String drawnStatus() const;
    void paint(juce::Graphics& g) override;

  private:
    MagicDustProcessor& processor_;
    std::array<KeyButton, 3> laneKeys_;
    KeyButton mainKey_;
    juce::String status_;

    MD_COMPONENT_BOILERPLATE(SenderBody)
};

} // namespace md::ui
