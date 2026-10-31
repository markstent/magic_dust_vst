#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/Boilerplate.h"
#include "ui/KeyButton.h"
#include "ui/TopLine.h"

namespace md::ui
{

// A down key, a cream window with the octave (-2 to +2) and an up key, bound to one octave
// parameter. The keys stop at the ends of the range; the window follows the host's automation.
class OctaveRocker : public juce::Component
{
  public:
    // lane names the part ("chords"), and index numbers the rocker for the component IDs
    // "octave0Down", "octave0Window" and "octave0Up".
    OctaveRocker(juce::AudioProcessorValueTreeState& params, const char* paramId,
                 const juce::String& lane, int index);

    // Re-reads the parameter into the window and the keys' enabled state.
    void refresh();

  private:
    [[nodiscard]] int current() const;
    void step(int delta);

    juce::AudioProcessorValueTreeState& params_;
    const char* paramId_;
    KeyButton down_;
    ReadoutWindow window_;
    KeyButton up_;

    MD_COMPONENT_BOILERPLATE(OctaveRocker)
};

} // namespace md::ui
