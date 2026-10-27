#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

// A "name value >" line that cycles a choice parameter on click, and on scroll when scrolls is set.
class FieldRow : public juce::Component
{
  public:
    FieldRow(juce::AudioProcessorValueTreeState& params, const char* id, bool scrolls);

    void refresh();
    // Moves the choice by steps, wrapping round.
    void cycle(int steps);
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

  private:
    juce::AudioParameterChoice& choice_;
    bool scrolls_;
    juce::String shown_;

    MD_COMPONENT_BOILERPLATE(FieldRow)
};

// The aura, groove and flavour lines. The rows carry the component IDs "groove" and "flavour".
class StatusFields : public juce::Component
{
  public:
    explicit StatusFields(juce::AudioProcessorValueTreeState& params);

    void refresh();
    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    juce::AudioProcessorValueTreeState& params_;
    FieldRow groove_;
    FieldRow flavour_;
    juce::String aura_;

    MD_COMPONENT_BOILERPLATE(StatusFields)
};

} // namespace md::ui
