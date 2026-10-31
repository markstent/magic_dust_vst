#pragma once

#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

// What a knob looks like. rangeWords are the two end words or empty; stepWords are one word per
// step on a stepped knob or empty; cap is the centre cap colour.
struct KnobLook
{
    juce::String legend;
    juce::StringArray rangeWords;
    juce::StringArray stepWords;
    juce::Colour cap;
    float diameter;
    std::optional<double> resetValue;
    float legendGap = 0.0F; // extra room between the legend and the face
    juce::String subLegend; // a small line under the legend, or empty
};

// A rotary control bound to one parameter. It draws its legend, tick scale, body, cap, pointer
// and words inside its own bounds, so the layout only has to give it a big enough box.
class Knob : public juce::Slider
{
  public:
    Knob(juce::AudioProcessorValueTreeState& params, const juce::String& paramId, KnobLook look);

    // The knob circle inside the component.
    [[nodiscard]] juce::Rectangle<int> faceBounds() const;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

  private:
    [[nodiscard]] double step() const;
    void anchor(const juce::MouseEvent& e);

    KnobLook look_;
    bool stepped_;
    bool fine_ = false;
    double anchorProportion_ = 0.0;
    double anchorUp_ = 0.0;
    float wheelAccumulated_ = 0.0F;
    juce::SliderParameterAttachment attachment_;

    MD_COMPONENT_BOILERPLATE(Knob)
};

} // namespace md::ui
