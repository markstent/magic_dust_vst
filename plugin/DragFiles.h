#pragma once

#include <cstddef>
#include <optional>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "md/types.hpp"

namespace md
{

// ~/Library/Caches/Magic Dust/drag/
juce::File defaultDragFolder();

// Writes the MIDI file for one chord (slot), one lane or, with neither, the whole idea, under the
// name dragFileName gives. Returns nothing when the file cannot be written.
std::optional<juce::File> writeDragFile(const Idea& idea, std::optional<LaneId> lane,
                                        std::optional<std::size_t> slot, int tempoBpm,
                                        const juce::File& folder = defaultDragFolder());

// Removes files in the folder that are more than 24 hours old.
void cleanDragFolder(const juce::File& folder = defaultDragFolder());

// The host's tempo rounded, or the default when it is unknown.
int dragTempo(std::optional<double> hostBpm);

// True once the pointer has moved far enough from where it went down to count as a drag.
bool exceedsDragThreshold(juce::Point<int> down, juce::Point<int> now);

} // namespace md
