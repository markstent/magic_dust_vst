#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Boilerplate.h"

namespace md::ui
{

class AuraPad : public juce::Component
{
  public:
    explicit AuraPad(juce::AudioProcessorValueTreeState& params);

    // Mood and energy, 0 to 100.
    void set(int mood, int energy);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

  private:
    void move(juce::Point<float> position);
    void write(const char* id, int value);

    juce::AudioProcessorValueTreeState& params_;
    int mood_ = 0;
    int energy_ = 0;

    MD_COMPONENT_BOILERPLATE(AuraPad)
};

} // namespace md::ui
