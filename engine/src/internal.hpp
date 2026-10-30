#pragma once

#include "md/pcg32.hpp"
#include "md/ranges.hpp"
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
//   chords: 0 entry, 1 progression, 2 chord rhythm cell, 3 velocity humanize, 4 turnaround cell,
//           5 ghosts, 6 lengths, 7 inversions
//   bass:   0 velocity humanize
//           1 pattern choice
//           2 pitch variation
//           3 approach notes
//           4 riff motif
//           5 call-and-response picks
//           6 ghosts
//           7 turnaround fill
//   melody: 0 rhythm cell A, 1 rhythm cell B, 2 pitch walk, 3 pluck tones, 4 velocity humanize,
//           5 answer cell, 6 answer pitches,
//           7 timing feel (drawn by performedNotes, not the generator)
// Entry, progression, mode and chord-cell choices belong to the chords lane.
constexpr std::uint32_t kChordEntry = 0;
constexpr std::uint32_t kChordProgression = 1;
constexpr std::uint32_t kChordCell = 2;
constexpr std::uint32_t kChordHumanize = 3;
constexpr std::uint32_t kChordTurnaround = 4;
constexpr std::uint32_t kChordGhost = 5;
constexpr std::uint32_t kChordLength = 6;
constexpr std::uint32_t kChordInversion = 7;
constexpr std::uint32_t kBassHumanize = 0;
constexpr std::uint32_t kBassPattern = 1;
constexpr std::uint32_t kBassPitch = 2;
constexpr std::uint32_t kBassApproach = 3;
constexpr std::uint32_t kBassRiff = 4;
constexpr std::uint32_t kBassCall = 5;
constexpr std::uint32_t kBassGhost = 6;
constexpr std::uint32_t kBassFill = 7;
constexpr std::uint32_t kMelodyCellA = 0;
constexpr std::uint32_t kMelodyCellB = 1;
constexpr std::uint32_t kMelodyWalk = 2;
constexpr std::uint32_t kMelodyPluck = 3;
constexpr std::uint32_t kMelodyHumanize = 4;
constexpr std::uint32_t kMelodyAnswerCell = 5;
constexpr std::uint32_t kMelodyAnswerWalk = 6;
constexpr std::uint32_t kMelodyFeel = 7; // drawn by performedNotes, not the generator

// Energy tiers: the turnaround and the answer phrase from kMidEnergy up; the bass fill and the
// breath above kHighEnergy.
constexpr int kMidEnergy = 30;
constexpr int kHighEnergy = 60;

// A ghost note's velocity, as a percent of the lane base.
constexpr int kGhostVelocity = 40;
// A floor on a varied chord hit's length, half a 16th. Today's 70% minimum never reaches it.
constexpr int kMinChordLength = kTicksPerSixteenth / 2;

// True for the bar (0-based) that ends a phrase and turns the idea around: the last bar, and bar 8
// of a 16-bar idea.
bool isTurnaroundBar(int bars, int bar);
// True for the bar that breathes: bar 4 of each 8 (a 4-bar idea has none).
bool isBreathBar(int bars, int bar);

// An inclusive pitch range.
struct PitchRange
{
    int low;
    int high;
};

// The 16th step of a swung start: an odd step lands in the second half of its pair of 16ths.
int stepOf(int start);

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
// The entry's modes that fit the whole progression (all of them for parallel entries), darkest
// first.
std::vector<Mode> fittingModes(const StyleEntry& entry, const Progression& progression);
// Places the progression over the idea's bars in its key, applies its flavour and sets the mode it
// borrows from. Voicings stay empty.
void placeChords(Idea& idea, const Progression& progression);

// The mode groups a progression model learns within.
enum class ModeGroup : std::uint8_t
{
    minor, // aeolian, dorian, phrygian
    major, // ionian, mixolydian
};
ModeGroup modeGroup(Mode mode);

// A first-order chain over the chord tokens of a family's progressions whose entries have a mode in
// one group. Tokens are indexed in order of first appearance.
struct ProgressionModel
{
    std::vector<ChordSymbol> tokens;
    std::vector<std::vector<int>> transitions; // counts [from][to], with each wrap to the first
    std::vector<int> starts;                   // counts of first chords
    std::vector<std::vector<int>> templates;   // the learned barsPerChord lists
    bool sevenths = false;                     // most learned tokens carry a 7th or a 9th
};

ProgressionModel buildProgressionModel(const Bank& bank, Family family, ModeGroup group);
// The plain diatonic chords of the entry's first mode that the model has not learned: 7ths when
// the model's tokens mostly carry one, otherwise triads. A degree with neither is left out.
std::vector<ChordSymbol> smoothingTokens(const ProgressionModel& model, const StyleEntry& entry);
// Draws a bar template, a first chord and a chain from the model, with smoothing: 0.15 added to
// every move between learned tokens and 0.05 for any move to or from a smoothing token. A chord
// never follows itself, the wrap included, unless the template has 1 chord. The result must fit
// one of the entry's modes and pass validate in every fitting mode, key and flavour. Gives up
// after 24 tries.
std::optional<Progression> inventProgression(const ProgressionModel& model, const StyleEntry& entry,
                                             Pcg32& rng);
// Above kHighEnergy callers move some voicings up one inversion (slot 7): a chord that repeats the
// previous span's symbol and reaches bar 3 or 4 of a 4-bar group (the voicing is one per span, so
// it moves for the whole span), when the new voicing keeps its top note in the entry's range and
// every voice within kMaxVoiceMove of both neighbours. Never for a parallel entry. Voicings must
// already be set.
void invertVoicings(const StyleEntry& entry, std::vector<PlacedChord>& chords,
                    std::uint64_t chordSeed);
// From kMidEnergy up a turnaround bar swaps in bar 1 of another of the entry's chord cells (slot
// 4). Above kHighEnergy the breath bar drops its last hit, hits of 2 sixteenths or fewer play 70,
// 85 or 100 percent of their length, at least 120 ticks (slot 6), and ghost notes are added (slot
// 5): at most one a bar and bars / 2 in all, never in a turnaround bar, each the bar's voicing on
// an odd 16th, 1 sixteenth long at kGhostVelocity.
std::vector<NoteEvent> chordEvents(const std::vector<PlacedChord>& chords, const RhythmCell& cell,
                                   const Settings& settings, const StyleEntry& entry,
                                   std::uint64_t chordSeed);

// The entry's allowed patterns run sparse to busy and the energy picks along them (slot 1).
BassPattern bassPatternFor(const StyleEntry& entry, int energy, Pcg32& rng);
// Pitch variation draws from Pcg32(bassSeed, kBassPitch), approach notes from kBassApproach, the
// riff motif from kBassRiff and the call-and-response picks from kBassCall. The call-and-response
// pattern reads the chords lane, which must already be in the idea.
// Above kHighEnergy the last bar's steps 8 to 15 become a fill (slot 7) of 3 or 4 sixteenths on
// steps 9, 10, 11, 13 or 14, walking scale tones to the first bass note, before the approach note.
// Velocity is a percent of the lane base: 100, or 85 for approach notes.
std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed);

// 1 to 3 of the allowed tones nearest the centre, cycled by the pluck.
// Throws std::logic_error when no tone is allowed.
std::vector<int> pluckPitches(const std::vector<int>& allowedPcs, Pcg32& rng);
std::vector<NoteEvent> melodyEvents(const StyleEntry& entry, const Idea& idea,
                                    std::uint64_t melodySeed);
} // namespace md::detail
