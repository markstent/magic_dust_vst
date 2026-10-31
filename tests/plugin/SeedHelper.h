#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PluginProcessor.h"

// Puts the processor on a known idea: the state's seed is restored and the lane seeds derived.
inline void setSeed(MagicDustProcessor& p, int seed)
{
    juce::MemoryBlock state;
    p.getStateInformation(state);
    auto xml =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    xml->setAttribute("seed", seed);
    xml->removeAttribute("chordsSeed");
    juce::MemoryBlock edited;
    juce::AudioProcessor::copyXmlToBinary(*xml, edited);
    p.setStateInformation(edited.getData(), static_cast<int>(edited.getSize()));
}
