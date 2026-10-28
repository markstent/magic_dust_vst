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
//           1 pattern choice
//           2 pitch variation
//           3 approach notes
//           4 riff motif
//           5 call-and-response picks
//   melody: 0 rhythm cell A, 1 rhythm cell B, 2 pitch walk, 3 pluck tones, 4 velocity humanize
// Entry, progression, mode and chord-cell choices belong to the chords lane.
constexpr std::uint32_t kChordEntry = 0;
constexpr std::uint32_t kChordProgression = 1;
constexpr std::uint32_t kChordCell = 2;
constexpr std::uint32_t kChordHumanize = 3;
constexpr std::uint32_t kBassHumanize = 0;
constexpr std::uint32_t kBassPattern = 1;
constexpr std::uint32_t kBassPitch = 2;
constexpr std::uint32_t kBassApproach = 3;
constexpr std::uint32_t kBassRiff = 4;
constexpr std::uint32_t kBassCall = 5;
constexpr std::uint32_t kMelodyCellA = 0;
constexpr std::uint32_t kMelodyCellB = 1;
constexpr std::uint32_t kMelodyWalk = 2;
constexpr std::uint32_t kMelodyPluck = 3;
constexpr std::uint32_t kMelodyHumanize = 4;

// An inclusive pitch range.
struct PitchRange
{
    int low;
    int high;
};

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

// A chord change a bass note can lead into: its approach tick and the pitch it leads to.
struct ApproachTarget
{
    int tick;
    int pitch;
};
// The one approach rule, for the generator, validator and refit. The bass must be sorted by start.
std::vector<ApproachTarget> approachTargets(const Idea& idea, std::span<const NoteEvent> bass);
// Start ticks of the bass notes that lead into a chord change (the validator's approach exemption).
std::vector<int> approachStarts(const Idea& idea);
// True when the note's pitch class is in the key scale or is a chord tone of the chord sounding at
// its start (the voicing's, for a parallel idea).
bool pitchInKeyOrChord(const Idea& idea, const NoteEvent& e);
// True when a bass note is a semitone off the chord sounding at its start, outside the pedal and
// approach exemptions.
bool bassClashes(const Idea& idea, const NoteEvent& e, std::span<const int> approaches);
// The pitch rules for a bass note: in range, in key or a chord tone, and no clash. Shared by the
// validator and the refit so they cannot disagree.
bool bassPitchOk(const Idea& idea, const NoteEvent& e, std::span<const int> approaches);

// True when a melody leap from one pitch to the next is within kMaxMelodyLeap or exactly an octave.
bool melodyLeapOk(int from, int to);
// True when a melody note starts on a quarter and its pitch class is not allowed against the chord
// sounding there.
bool melodyStrongBeatClash(const Idea& idea, const NoteEvent& e);
// The pitch rules for one melody note, apart from its leap: in range, in key or a chord tone, and
// no strong-beat clash. Shared by the validator and the refit so they cannot disagree.
bool melodyPitchOk(const Idea& idea, const NoteEvent& e);

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

// The entry's allowed patterns run sparse to busy and the energy picks along them (slot 1).
BassPattern bassPatternFor(const StyleEntry& entry, int energy, Pcg32& rng);
// Pitch variation draws from Pcg32(bassSeed, kBassPitch), approach notes from kBassApproach, the
// riff motif from kBassRiff and the call-and-response picks from kBassCall. The call-and-response
// pattern reads the chords lane, which must already be in the idea.
// Velocity is a percent of the lane base: 100, or 85 for approach notes.
std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed);

// 1 to 3 of the allowed tones nearest the centre, cycled by the pluck.
// Throws std::logic_error when no tone is allowed.
std::vector<int> pluckPitches(const std::vector<int>& allowedPcs, Pcg32& rng);
std::vector<NoteEvent> melodyEvents(const StyleEntry& entry, const Idea& idea,
                                    std::uint64_t melodySeed);
} // namespace md::detail
