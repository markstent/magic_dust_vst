#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"
#include "ui/KeyButton.h"

class MagicDustProcessor;

namespace md::ui
{

// A keycap with just its label, centred.
class LabelKey : public KeyButton
{
  public:
    LabelKey(const juce::String& label, std::optional<juce::Colour> led);

  protected:
    void paintContent(juce::Graphics& g, juce::Rectangle<float> face) const override;

    MD_COMPONENT_BOILERPLATE(LabelKey)
};

// What a sender instance shows, at its logical 520 x 240 size: pick the lane it sends, or become
// a full Magic Dust.
class SenderBody : public juce::Component
{
  public:
    explicit SenderBody(MagicDustProcessor& processor);

    void refresh();
    [[nodiscard]] const juce::String& status() const { return status_; }
    void paint(juce::Graphics& g) override;

  private:
    MagicDustProcessor& processor_;
    std::array<LabelKey, 3> laneKeys_;
    LabelKey mainKey_;
    juce::String status_;

    MD_COMPONENT_BOILERPLATE(SenderBody)
};

} // namespace md::ui
