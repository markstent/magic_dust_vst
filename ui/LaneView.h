#pragma once

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "md/types.hpp"

#include "ui/Boilerplate.h"

namespace md::ui
{

struct LaneLook
{
    const char* label;
    juce::Colour colour;
    int low;
    int high;
};

// Label, colour and pitch range of each lane.
LaneLook laneLook(md::LaneId lane);

class PianoRoll : public juce::Component
{
  public:
    explicit PianoRoll(md::LaneId lane);

    void set(const std::vector<md::NoteEvent>& notes, int bars, std::optional<float> playhead);
    // The notes' rectangles in this component's own coordinates.
    [[nodiscard]] std::vector<juce::Rectangle<float>> noteRects() const;
    void paint(juce::Graphics& g) override;

  private:
    md::LaneId lane_;
    std::vector<md::NoteEvent> notes_;
    int bars_ = 8;
    std::optional<float> playhead_;

    MD_COMPONENT_BOILERPLATE(PianoRoll)
};

class LaneView : public juce::Component
{
  public:
    LaneView(md::LaneId lane, juce::AudioProcessorValueTreeState& params);

    // playhead is the fraction of the idea played, when playing.
    void set(const md::Lane& lane, int bars, std::optional<float> playhead, bool selected);
    [[nodiscard]] PianoRoll& roll() { return roll_; }
    [[nodiscard]] juce::Slider& levelSlider() { return level_; }
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType /*cause*/) override { repaint(); }
    void focusLost(FocusChangeType /*cause*/) override { repaint(); }

    // Called on a click, or Space or Enter while focused.
    void setOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }
    // Called once the pointer has moved far enough to count as a drag.
    void setOnDrag(std::function<void()> callback) { onDrag_ = std::move(callback); }

  private:
    md::LaneId lane_;
    PianoRoll roll_;
    juce::Slider level_{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
    juce::SliderParameterAttachment levelAttachment_;
    bool selected_ = false;
    bool dragging_ = false;
    std::function<void()> onClick_;
    std::function<void()> onDrag_;

    MD_COMPONENT_BOILERPLATE(LaneView)
};

} // namespace md::ui
