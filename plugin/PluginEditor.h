#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class MagicDustProcessor;

class MagicDustEditor : public juce::AudioProcessorEditor
{
  public:
    explicit MagicDustEditor(MagicDustProcessor& owner);

    void paint(juce::Graphics& g) override;
};
