#pragma once

#include "md/pcg32.hpp"
#include "md/style.hpp"
#include "md/types.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

namespace md::detail
{
// Every random draw is Pcg32(<lane seed>, slot) with one fixed slot per use:
//   chords: 0 entry, 1 progression, 2 chord rhythm cell, 3 velocity humanize
//   bass:   0 velocity humanize
//   melody: 0 rhythm cell A, 1 rhythm cell B, 2 pitch walk, 3 pluck tones, 4 velocity humanize
// Entry, progression, mode and chord-cell choices belong to the chords lane.
constexpr std::uint32_t kChordEntry = 0;
constexpr std::uint32_t kChordProgression = 1;
constexpr std::uint32_t kChordCell = 2;
constexpr std::uint32_t kChordHumanize = 3;
constexpr std::uint32_t kBassHumanize = 0;
constexpr std::uint32_t kMelodyCellA = 0;
constexpr std::uint32_t kMelodyCellB = 1;
constexpr std::uint32_t kMelodyWalk = 2;
constexpr std::uint32_t kMelodyPluck = 3;
constexpr std::uint32_t kMelodyHumanize = 4;

// Odd sixteenth steps land late: start = pairStart + 480 * swingPermille / 1000.
int swung(int start, int swingPermille);

// The chord sounding at the tick, or null when none does.
const PlacedChord* findChord(std::span<const PlacedChord> progression, int tick);
// As findChord, but throws std::logic_error when none sounds.
const PlacedChord& chordAt(std::span<const PlacedChord> progression, int tick);

// The pitch classes sounding for a chord: its symbol's, or its voicing's when the idea is parallel
// (the shape moves with the root and may leave the symbol).
std::vector<int> soundingPitchClasses(const Idea& idea, const PlacedChord& chord);

// The pitch classes sounding at the tick, as soundingPitchClasses; empty when no chord sounds.
std::vector<int> soundingPcsAt(const Idea& idea, int tick);

// The pitch classes at least 2 semitones from every other one in the set.
std::vector<int> allowedPitchClasses(const std::vector<int>& pcs);

// True when a voicing has an interval below a perfect 4th under MIDI 52 (which rules out 3rds
// under 48 too).
bool isMuddy(const Voicing& voicing);

// Calls f(voice, move) for each pair of voices between two voicings: the top voices pair with each
// other and the rest pair from the bottom, min(size) - 1 of them; a larger chord's extra voices are
// unpaired. voice is the index in a; move is the distance in semitones.
template <class F> void forEachVoicePair(const Voicing& a, const Voicing& b, const F& f)
{
    const std::size_t paired = std::min(a.size(), b.size()) - 1;
    for (std::size_t i = 0; i < paired; ++i)
    {
        f(i, std::abs(a.at(i) - b.at(i)));
    }
    f(a.size() - 1, std::abs(a.back() - b.back()));
}

// Ends each note at the next onset and at endTick, and drops notes left shorter than 1 tick.
// For monophonic lanes whose events are sorted by start.
void clampMonophonic(std::vector<NoteEvent>& events, int endTick);

// Soulful keeps the written chord. Simple steps it down to a triad or 7th (add9 to maj). Jazzy
// climbs up to two steps (min to min11, maj to maj9 or via dom7, dom7 to dom13), each needing every
// note in the key's scale or the borrowed mode's; sus4 and six never change.
ChordSymbol applyFlavour(const Key& key, std::optional<Mode> borrowedFrom, const ChordSymbol& chord,
                         Flavour flavour);

// Tiles the progression's chords from bar 0 and truncates the last at bars. Voicings stay empty.
std::vector<PlacedChord> placeProgression(const Progression& progression, int bars);
void voiceChords(const StyleEntry& entry, const Key& key, std::vector<PlacedChord>& chords);
std::vector<NoteEvent> chordEvents(const std::vector<PlacedChord>& chords, const RhythmCell& cell,
                                   const Settings& settings);

BassPattern bassPatternFor(const StyleEntry& entry);
std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern);

// 1 to 3 of the allowed tones nearest the centre, cycled by the pluck.
// Throws std::logic_error when no tone is allowed.
std::vector<int> pluckPitches(const std::vector<int>& allowedPcs, Pcg32& rng);
std::vector<NoteEvent> melodyEvents(const StyleEntry& entry, const Idea& idea,
                                    std::uint64_t melodySeed);
} // namespace md::detail
