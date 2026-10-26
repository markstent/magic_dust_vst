#pragma once

#include "md/types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace md
{
struct MidiTrack
{
    std::string name;
    std::vector<NoteEvent> notes;
    std::vector<std::pair<int, std::string>> markers; // tick, text
};

// Format 0 for one track, format 1 (with a conductor track) for more.
// Throws std::invalid_argument on bad input.
std::vector<std::uint8_t> writeMidiFile(std::span<const MidiTrack> tracks, int tempoBpm,
                                        int totalTicks);

int ideaLengthTicks(const Idea& idea);
// Muted lanes are still exported: muting is preview-only.
MidiTrack laneTrack(const Idea& idea, LaneId lane);
std::vector<MidiTrack> ideaTracks(const Idea& idea);
} // namespace md
