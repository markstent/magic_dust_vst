#pragma once

#include <functional>
#include <utility>

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
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // Called once the pointer has moved far enough to count as a drag.
    void setOnDrag(std::function<void()> callback) { onDrag_ = std::move(callback); }

  private:
    int slot_;
    juce::String name_;
    juce::String feel_;
    bool playing_ = false;
    bool selected_ = false;
    bool dragging_ = false;
    std::function<void()> onDrag_;

    MD_COMPONENT_BOILERPLATE(ChordBubble)
};

} // namespace md::ui
