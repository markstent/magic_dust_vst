#pragma once

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

// The face gradient, the hard shadow below it and the corner radius; the sprinkle key has its own.
struct KeyMaterial
{
    juce::Colour top;
    juce::Colour bottom;
    float radius;
    float shadowDrop;
};

// A key with a hard shadow below, a legend under it and an optional LED above. Pressing moves it
// down 1 px and the shadow shrinks to match.
class KeyButton : public juce::Button, private juce::Timer
{
  public:
    // The label is drawn in capitals under the key.
    KeyButton(const juce::String& label, std::optional<juce::Colour> led);

    // A subclass that draws its own text inside the key turns the legend below off.
    void setLegendBelow(bool below) { legendBelow_ = below; }
    void setLit(bool lit);
    [[nodiscard]] bool isLit() const { return lit_; }
    // Off, the key and its LED jump instead of animating.
    void setAnimated(bool animated);
    // 0 at rest, 1 fully pressed.
    [[nodiscard]] float pressedAmount() const { return press_; }
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    bool keyPressed(const juce::KeyPress& key) override;

  protected:
    KeyButton(const juce::String& label, const KeyMaterial& material,
              std::optional<juce::Colour> led);
    // Paints the key's content inside its face; nothing by default.
    virtual void paintContent(juce::Graphics& g, juce::Rectangle<float> face) const;

  private:
    void buttonStateChanged() override;
    void timerCallback() override;
    void retarget();

    KeyMaterial material_;
    juce::String label_;
    std::optional<juce::Colour> led_;
    bool legendBelow_ = true;
    bool lit_ = false;
    bool animated_ = true;
    float press_ = 0.0F;
    float ledLevel_ = 0.0F;
    double lastTickMs_ = 0.0;

    MD_COMPONENT_BOILERPLATE(KeyButton)
};

} // namespace md::ui
