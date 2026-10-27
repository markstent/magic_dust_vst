#pragma once

#include <cstdint>
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
    juce::Colour shadow;
    float radius;
    float shadowDrop;
};

// What a dark key shows inside its face; a cream key shows nothing. A disabled key draws at 40%.
enum class KeyIcon : std::uint8_t
{
    none,
    play,
    stop,
    arrowDown,
    arrowUp
};

// A key with a hard shadow below, a legend under it and an optional LED above. Pressing moves it
// down 1 px and the shadow shrinks to match.
class KeyButton : public juce::Button, private juce::Timer
{
  public:
    // The label is drawn in capitals under the key; an empty one draws nothing. An icon makes the
    // key dark and draws the icon inside it.
    KeyButton(const juce::String& label, std::optional<juce::Colour> led,
              KeyIcon icon = KeyIcon::none);

    // Where the legend is drawn, in the key's own coordinates.
    [[nodiscard]] juce::Rectangle<float> legendBounds() const;
    void setLit(bool lit);
    [[nodiscard]] bool isLit() const { return lit_; }
    // Off, the key and its LED jump instead of animating.
    void setAnimated(bool animated);
    // 0 at rest, 1 fully pressed.
    [[nodiscard]] float pressedAmount() const { return press_; }
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;
    bool keyPressed(const juce::KeyPress& key) override;
    // A toggling key's LED follows its state, however the state changed.
    void clicked() override;

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
    KeyIcon icon_ = KeyIcon::none;
    juce::String label_;
    std::optional<juce::Colour> led_;
    bool lit_ = false;
    bool animated_ = true;
    float press_ = 0.0F;
    float ledLevel_ = 0.0F;
    double lastTickMs_ = 0.0;

    MD_COMPONENT_BOILERPLATE(KeyButton)
};

} // namespace md::ui
