#pragma once

#include "ui/Boilerplate.h"
#include "ui/KeyButton.h"

namespace md::ui
{

// The wide red key with "SPRINKLE" in white inside it.
class SprinkleKey : public KeyButton
{
  public:
    SprinkleKey();

  protected:
    void paintContent(juce::Graphics& g, juce::Rectangle<float> face) const override;

    MD_COMPONENT_BOILERPLATE(SprinkleKey)
};

} // namespace md::ui
