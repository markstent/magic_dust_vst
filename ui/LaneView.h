#pragma once

#include <optional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "md/types.hpp"

#include "ui/Boilerplate.h"

namespace md::ui
{

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
    explicit LaneView(md::LaneId lane);

    // playhead is the fraction of the idea played, when playing.
    void set(const md::Lane& lane, int bars, std::optional<float> playhead, bool selected);
    [[nodiscard]] PianoRoll& roll() { return roll_; }
    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    md::LaneId lane_;
    PianoRoll roll_;
    bool selected_ = false;

    MD_COMPONENT_BOILERPLATE(LaneView)
};

} // namespace md::ui
