#pragma once

#include <cstddef>
#include <optional>

#include <juce_core/juce_core.h>

#include "md/types.hpp"

namespace md
{

// ~/Library/Caches/Magic Dust/drag/
juce::File defaultDragFolder();

// Writes the MIDI file for one chord (slot) or one lane, under the name dragFileName gives.
// Returns nothing when the file cannot be written, the slot is missing or neither is given.
std::optional<juce::File> writeDragFile(const Idea& idea, std::optional<LaneId> lane,
                                        std::optional<std::size_t> slot, int tempoBpm,
                                        const juce::File& folder = defaultDragFolder());

// Removes .mid files in the folder that are more than 24 hours old; nothing else is touched.
void cleanDragFolder(const juce::File& folder = defaultDragFolder());

// The host's tempo rounded, or the default when it is unknown.
int dragTempo(std::optional<double> hostBpm);

} // namespace md
