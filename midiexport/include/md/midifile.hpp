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

// The timing feel of one note, in ticks: a performance layer that never changes the idea's events.
//   chords: a lay-back of 2, 5, 8 or 10 for groove straight, 54, 58 or 62
//   bass:   0, always locked to the kick
//   melody: a seeded offset of +-(2 + groove * 1.5) rounded (lane seed, slot 7, one draw per note)
int feelOffsetTicks(const Idea& idea, LaneId lane, std::size_t noteIndex);
// The lane's notes as they play, with the feel applied. Never before tick 0, a melody note ends
// before the next one starts, and a note that would cross the idea's end is shortened.
// Playback and every MIDI export build their notes from this.
std::vector<NoteEvent> performedNotes(const Idea& idea, LaneId lane);
// Muted lanes are still exported: muting is preview-only.
// Chords are on channel 1, bass on 2 and melody on 3, as in the live MIDI output.
MidiTrack laneTrack(const Idea& idea, LaneId lane);
std::vector<MidiTrack> ideaTracks(const Idea& idea);

// Shifts every note by 12 semitones per octave. A note that would leave MIDI's 0 to 127 is dropped,
// not clamped: clamping would play it at the wrong pitch, on top of another note.
MidiTrack shiftOctaves(MidiTrack track, int octaves);

// The chord lane notes that start inside placed chord `slot`, shifted so the chord starts at
// tick 0 and clipped to its length. Throws std::out_of_range for a missing slot.
MidiTrack chordSlotTrack(const Idea& idea, std::size_t slot);
// "<Chords|Bass|Melody|Chord|Magic Dust> - <chords> - <tempo>bpm.mid". A slot means one chord and
// ignores the lane; no lane and no slot means the whole idea.
std::string dragFileName(const Idea& idea, std::optional<LaneId> lane,
                         std::optional<std::size_t> slot, int tempoBpm);
} // namespace md
