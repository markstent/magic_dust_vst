#pragma once

#include "md/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
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
    int channel = 0;                                  // 0 to 15, shown as channel 1 to 16
};

// Format 0 for one track, format 1 (with a conductor track) for more.
// Throws std::invalid_argument on bad input.
std::vector<std::uint8_t> writeMidiFile(std::span<const MidiTrack> tracks, int tempoBpm,
                                        int totalTicks);

int ideaLengthTicks(const Idea& idea);
// Muted lanes are still exported: muting is preview-only.
// Chords are on channel 1, bass on 2 and melody on 3, as in the live MIDI output.
MidiTrack laneTrack(const Idea& idea, LaneId lane);
std::vector<MidiTrack> ideaTracks(const Idea& idea);

// The chord lane notes that start inside placed chord `slot`, shifted so the chord starts at
// tick 0 and clipped to its length. Throws std::out_of_range for a missing slot.
MidiTrack chordSlotTrack(const Idea& idea, std::size_t slot);
// "<Chords|Bass|Melody|Chord|Magic Dust> - <chords> - <tempo>bpm.mid". A slot means one chord and
// ignores the lane; no lane and no slot means the whole idea.
std::string dragFileName(const Idea& idea, std::optional<LaneId> lane,
                         std::optional<std::size_t> slot, int tempoBpm);
} // namespace md
