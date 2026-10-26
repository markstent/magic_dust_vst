#pragma once

#include "md/style.hpp"
#include "md/types.hpp"

#include <span>
#include <vector>

namespace md::detail
{
constexpr int kStepsPerBar = 16;

// Odd sixteenth steps land late: start = pairStart + 480 * swingPermille / 1000.
int swung(int start, int swingPermille);

// The chord sounding at the tick, or null when none does.
const PlacedChord* findChord(std::span<const PlacedChord> progression, int tick);
// As findChord, but throws std::logic_error when none sounds.
const PlacedChord& chordAt(std::span<const PlacedChord> progression, int tick);

// The pitch classes sounding for a chord: its symbol's, or its voicing's when the idea is parallel
// (the shape moves with the root and may leave the symbol).
std::vector<int> soundingPitchClasses(const Idea& idea, const PlacedChord& chord);

// The pitch classes at least 2 semitones from every other one in the set.
std::vector<int> allowedPitchClasses(const std::vector<int>& pcs);

// True when a voicing has an interval below a perfect 4th under MIDI 52 (which rules out 3rds
// under 48 too).
bool isMuddy(const Voicing& voicing);

// Ends each note at the next onset and at endTick, and drops notes left shorter than 1 tick.
// For monophonic lanes whose events are sorted by start.
void clampMonophonic(std::vector<NoteEvent>& events, int endTick);

// Tiles the progression's chords from bar 0 and truncates the last at bars. Voicings stay empty.
std::vector<PlacedChord> placeProgression(const Progression& progression, int bars);
void voiceChords(const StyleEntry& entry, const Key& key, std::vector<PlacedChord>& chords);
std::vector<NoteEvent> chordEvents(const std::vector<PlacedChord>& chords, const RhythmCell& cell,
                                   const Settings& settings);

BassPattern bassPatternFor(const StyleEntry& entry);
std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern);

std::vector<NoteEvent> melodyEvents(const StyleEntry& entry, const Idea& idea);
} // namespace md::detail
