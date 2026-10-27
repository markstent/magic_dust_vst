#pragma once

#include "ui/Boilerplate.h"
#include "ui/KeyButton.h"

namespace md::ui
{

// The wide gold key: a sparkle, "sprinkle" and "new idea".
class SprinkleKey : public KeyButton
{
  public:
    SprinkleKey();

  protected:
    void paintContent(juce::Graphics& g, juce::Rectangle<float> face) const override;

    MD_COMPONENT_BOILERPLATE(SprinkleKey)
};

} // namespace md::ui
