#pragma once

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

// The colours and soft shadow of a keycap; the gold key has its own.
struct KeyMaterial
{
    juce::Colour top;
    juce::Colour bottom;
    juce::Colour edge;
    juce::Colour base;
    float shadowBlur;
    float shadowAlpha;
};

// A key with a solid base, a soft shadow and an optional LED. Pressing moves it down 3 px.
class KeyButton : public juce::Button, private juce::Timer
{
  public:
    // iconPath is an SVG path on a 24 x 24 grid, filled; iconSize is its size in px.
    KeyButton(const juce::String& label, const char* iconPath, float iconSize,
              std::optional<juce::Colour> led);

    void setLit(bool lit);
    // Off, the key and its LED jump instead of animating.
    void setAnimated(bool animated);
    // 0 at rest, 1 fully pressed.
    [[nodiscard]] float pressedAmount() const { return press_; }
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  protected:
    KeyButton(const juce::String& label, const KeyMaterial& material,
              std::optional<juce::Colour> led);
    // Paints the key's content inside its face.
    virtual void paintContent(juce::Graphics& g, juce::Rectangle<float> face) const;

  private:
    void buttonStateChanged() override;
    void timerCallback() override;
    void retarget();

    KeyMaterial material_;
    juce::String label_;
    juce::Path icon_;
    float iconSize_ = 0.0F;
    std::optional<juce::Colour> led_;
    bool lit_ = false;
    bool animated_ = true;
    float press_ = 0.0F;
    float ledLevel_ = 0.0F;
    double lastTickMs_ = 0.0;

    MD_COMPONENT_BOILERPLATE(KeyButton)
};

} // namespace md::ui
